// What `--stats` and `--export` put on stdout.
//
// Both read the history and neither computes anything, so what is under test
// here is presentation: that a person with no history gets a sentence rather
// than a table of zeros, that a script gets output it can actually parse, and
// that two invocations over one database agree to the byte.

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <gtest/gtest.h>
#include <initializer_list>
#include <string>
#include <string_view>
#include <vector>

#include "typeit/app/records/History.h"
#include "typeit/app/services/HistoryService.h"
#include "typeit/cli/Cli.h"
#include "typeit/cli/Reports.h"
#include "typeit/core/util/Result.h"
#include "typeit/core/util/Units.h"
#include "typeit/testing/Fakes.h"

namespace typeit::cli {
    namespace {

        /// Noon on 1 January 2026, the fixed instant every history test uses.
        constexpr core::Millis kNoon{1'767'225'600'000};
        constexpr std::int64_t kDay = 86'400'000;

        /// How many lines the text has, so a limit can be checked without
        /// parsing the format it limited.
        std::size_t lines_in(std::string_view text) { return static_cast<std::size_t>(std::ranges::count(text, '\n')); }

        /// One `key: value` line, or empty.
        std::string field(std::string_view text, std::string_view key) {
            const std::string needle = std::string{key} + ": ";
            const std::size_t at = text.find(needle);
            if (at == std::string_view::npos) {
                return {};
            }
            const std::size_t from = at + needle.size();
            return std::string{text.substr(from, text.find('\n', from) - from)};
        }

        class ExportCliTest : public ::testing::Test {
        protected:
            [[nodiscard]] static app::SessionRecord a_run(core::Millis at, double net_wpm) {
                app::SessionRecord record;
                record.started_at = at;
                record.ended_at = at + core::Millis{30'000};
                record.mode = "timed";
                record.mode_param = R"({"seconds":30})";
                record.provider = "whole";
                record.duration = core::Millis{30'000};
                record.graphemes_typed = 300;
                record.net_wpm = core::Wpm{net_wpm};
                record.gross_wpm = core::Wpm{net_wpm + 10.0};
                record.accuracy = core::Accuracy{0.98};
                record.consistency = 90.0;
                record.completed = true;
                record.app_version = "test";
                return record;
            }

            void given_runs_on(std::initializer_list<std::int64_t> days_ago) {
                double wpm = 60.0;
                for (const std::int64_t ago: days_ago) {
                    ASSERT_TRUE(history_.save(a_run(core::Millis{kNoon.value - (ago * kDay)}, wpm)));
                    wpm += 10.0;
                }
            }

            [[nodiscard]] static CliOptions with_last(std::int64_t last) {
                CliOptions options;
                options.last = last;
                return options;
            }

            [[nodiscard]] std::string stats(const CliOptions& options = {}) {
                const core::Result<std::string> text = stats_summary(service_, history_, options, kNoon);
                EXPECT_TRUE(text) << (text ? "" : text.error().context);
                return text.value_or(std::string{});
            }

            [[nodiscard]] std::string exported(std::string_view format, const CliOptions& base = {}) {
                CliOptions options = base;
                options.action = Action::Export;
                options.operand = format;
                const core::Result<std::string> text = export_history(service_, options);
                EXPECT_TRUE(text) << (text ? "" : text.error().context);
                return text.value_or(std::string{});
            }

            testing::FakeHistoryRepository history_;
            app::HistoryService service_{history_};
        };

        // --- --stats --------------------------------------------------------

        TEST_F(ExportCliTest, AnEmptyHistoryGetsASentenceRatherThanATableOfZeros) {
            const std::string text = stats();

            EXPECT_EQ(text, "No sessions yet. Finish a run and it will show up here.\n");
            EXPECT_EQ(text.find("0%"), std::string::npos) << "nobody needs to be told their mean accuracy is 0%";
        }

        TEST_F(ExportCliTest, TheTotalsAreReported) {
            given_runs_on({0, 1, 2});

            const std::string text = stats();

            EXPECT_EQ(field(text, "sessions"), "3");
            EXPECT_EQ(field(text, "mean wpm"), "70");
            EXPECT_EQ(field(text, "best wpm"), "80");
            EXPECT_EQ(field(text, "worst wpm"), "60");
            EXPECT_EQ(field(text, "mean accuracy"), "98%");
            EXPECT_EQ(field(text, "graphemes"), "900");
        }

        TEST_F(ExportCliTest, TimeIsReadableRatherThanMilliseconds) {
            // Three thirty-second runs is a minute and a half.
            given_runs_on({0, 1, 2});

            EXPECT_EQ(field(stats(), "time"), "1m 30s");
        }

        TEST_F(ExportCliTest, TheStreakIsCountedInDays) {
            given_runs_on({0, 1, 2});

            const std::string text = stats();

            EXPECT_EQ(field(text, "current"), "3 days");
            EXPECT_EQ(field(text, "longest"), "3 days");
        }

        TEST_F(ExportCliTest, OneDayIsADayNotOneDays) {
            given_runs_on({0});

            EXPECT_EQ(field(stats(), "current"), "1 day");
        }

        TEST_F(ExportCliTest, PersonalBestsNameTheirModeAndParameter) {
            // A 15-second best and a 60-second best are different records, and
            // a line that said only "net_wpm" would collapse them.
            given_runs_on({0, 1});

            const std::string text = stats();

            EXPECT_NE(text.find("[personal bests]"), std::string::npos) << text;
            EXPECT_NE(text.find(R"(timed {"seconds":30} net_wpm: )"), std::string::npos) << text;
        }

        TEST_F(ExportCliTest, TheStatsAreByteIdenticalAcrossRuns) {
            given_runs_on({0, 1, 2});

            EXPECT_EQ(stats(), stats());
        }

        TEST_F(ExportCliTest, AFailureToReadTheHistoryIsReported) {
            history_.failure.fail_next(core::make_error(core::ErrorCode::DbQuery, "the disk is on fire"));

            const core::Result<std::string> text = stats_summary(service_, history_, CliOptions{}, kNoon);

            ASSERT_FALSE(text);
            EXPECT_EQ(text.error().code, core::ErrorCode::DbQuery);
        }

        // --- --export -------------------------------------------------------

        TEST_F(ExportCliTest, CsvHasAHeaderAndARowPerSession) {
            given_runs_on({0, 1, 2});

            const std::string text = exported("csv");

            EXPECT_TRUE(text.starts_with("id,")) << text;
            EXPECT_EQ(lines_in(text), 4U) << "a header and three rows";
        }

        TEST_F(ExportCliTest, JsonIsAnArrayOfObjects) {
            given_runs_on({0, 1});

            const std::string text = exported("json");

            EXPECT_TRUE(text.starts_with("[")) << text;
            EXPECT_TRUE(text.ends_with("]")) << text;
            EXPECT_NE(text.find(R"("net_wpm":)"), std::string::npos) << text;
        }

        TEST_F(ExportCliTest, AnEmptyHistoryExportsSomethingAParserCanRead) {
            // Not a special case and not an error: a script that runs nightly
            // should not fall over on the first night.
            EXPECT_TRUE(exported("csv").starts_with("id,"));
            EXPECT_EQ(exported("json"), "[]");
        }

        TEST_F(ExportCliTest, LastLimitsTheRows) {
            given_runs_on({0, 1, 2, 3, 4});

            EXPECT_EQ(lines_in(exported("csv", with_last(2))), 3U) << "a header and two rows";
        }

        TEST_F(ExportCliTest, ALimitLargerThanTheHistoryReturnsEverything) {
            given_runs_on({0, 1});

            EXPECT_EQ(lines_in(exported("csv", with_last(100))), 3U);
        }

        TEST_F(ExportCliTest, WithoutALastEverythingIsExported) {
            given_runs_on({0, 1, 2, 3, 4});

            EXPECT_EQ(lines_in(exported("csv")), 6U);
            EXPECT_EQ(filter_from(CliOptions{}).limit, 0U) << "zero means unlimited";
        }

        TEST_F(ExportCliTest, TheExportIsByteIdenticalAcrossRuns) {
            given_runs_on({0, 1, 2});

            EXPECT_EQ(exported("csv"), exported("csv"));
            EXPECT_EQ(exported("json"), exported("json"));
        }

        TEST_F(ExportCliTest, AFormatTheParserWouldHaveRefusedIsStillRefusedHere) {
            // Unreachable through the command line, and reported rather than
            // silently defaulted: exporting the wrong thing without saying so
            // is worse than exporting nothing.
            CliOptions options;
            options.action = Action::Export;
            options.operand = "xml";

            const core::Result<std::string> text = export_history(service_, options);

            ASSERT_FALSE(text);
            EXPECT_EQ(text.error().code, core::ErrorCode::InvalidArgument);
        }

        TEST_F(ExportCliTest, AFailureToReadTheHistoryReachesTheExport) {
            history_.failure.fail_next(core::make_error(core::ErrorCode::DbQuery, "no"));

            CliOptions options;
            options.action = Action::Export;
            options.operand = "csv";

            EXPECT_FALSE(export_history(service_, options));
        }

    }  // namespace
}  // namespace typeit::cli
