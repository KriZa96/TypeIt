// The export is valid JSON, checked by a parser rather than by eye (TI-069).
//
// `app` has no JSON parser and does not want one for a writer this small, but
// SQLite — which infra already links — has `json_valid()`, and a test living in
// the contract directory can see both layers. So the assertion the issue asks
// for is a real parse by a real parser, and it costs one dependency the
// project already pays for.

#include <gtest/gtest.h>
#include <string>
#include <utility>
#include <vector>

#include "typeit/app/records/History.h"
#include "typeit/app/services/HistoryService.h"
#include "typeit/core/util/Result.h"
#include "typeit/core/util/Units.h"
#include "typeit/infra/db/SqliteDatabase.h"
#include "typeit/testing/Fakes.h"

namespace typeit {
    namespace {

        /// True when SQLite can parse it. `json_valid` is the whole point: an
        /// eyeball assertion on a string that happens to contain the right
        /// substrings would pass on output no parser accepts.
        bool is_valid_json(const std::string& text) {
            core::Result<infra::SqliteDatabase> database = infra::SqliteDatabase::open_in_memory();
            EXPECT_TRUE(database) << (database ? "" : database.error().context);
            if (!database) {
                return false;
            }
            core::Result<infra::Statement> statement = database->prepare("SELECT json_valid(?1)");
            EXPECT_TRUE(statement) << (statement ? "" : statement.error().context);
            if (!statement) {
                return false;
            }
            const core::Result<bool> row = statement->bind(1, text).step();
            EXPECT_TRUE(row) << (row ? "" : row.error().context);
            return row.value_or(false) && statement->column_int(0) == 1;
        }

        app::SessionRecord a_run(std::string mode_param) {
            app::SessionRecord record;
            record.mode = "quote";
            record.mode_param = std::move(mode_param);
            record.completed = true;
            record.started_at = core::Millis{1'767'225'600'000};
            record.duration = core::Millis{30'000};
            record.net_wpm = core::Wpm{72.5};
            record.gross_wpm = core::Wpm{74.5};
            record.accuracy = core::Accuracy{0.9812};
            record.consistency = 81.25;
            return record;
        }

        std::string exported(const std::vector<std::string>& params) {
            testing::FakeHistoryRepository history;
            for (const std::string& param: params) {
                EXPECT_TRUE(history.save(a_run(param)));
            }
            app::HistoryService service{history};
            const core::Result<std::string> json = service.to_json({});
            EXPECT_TRUE(json) << (json ? "" : json.error().context);
            return json.value_or("<error>");
        }

        TEST(HistoryExportTest, AnEmptyExportParses) { EXPECT_TRUE(is_valid_json(exported({}))); }

        TEST(HistoryExportTest, AnOrdinaryExportParses) {
            EXPECT_TRUE(is_valid_json(exported({R"({"seconds":30})", R"({"words":50})"})));
        }

        TEST(HistoryExportTest, TheNastyStringsStillParse) {
            const std::string json = exported({
                    R"(a "quoted" string)",
                    R"(a \ backslash)",
                    "a\nnewline\tand a tab",
                    "a \x01 control character",
                    "čšž 漢字 — non-ASCII",
            });

            ASSERT_TRUE(is_valid_json(json)) << json;
            EXPECT_NE(json.find("čšž 漢字"), std::string::npos) << "and UTF-8 survived the round trip";
        }

    }  // namespace
}  // namespace typeit
