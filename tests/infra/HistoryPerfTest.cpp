// The history screen's budget, enforced (TI-109, ARCHITECTURE §6.5).
//
// Two assertions, and the second is the one that keeps the first true. A wall
// clock says the load is fast enough *on this machine today*; `EXPLAIN QUERY
// PLAN` says the database is using an index rather than reading every row, and
// that is the property a slower machine, a bigger history and a future
// refactor all have to keep.
//
// The database is seeded by this fixture, never committed. Ten thousand
// sessions is about eight years of daily practice — the point is not that
// anybody has one, but that the load must not get slower in proportion to how
// much somebody has typed.

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <gtest/gtest.h>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "typeit/app/records/History.h"
#include "typeit/app/services/HistoryService.h"
#include "typeit/core/util/Result.h"
#include "typeit/core/util/Units.h"
#include "typeit/infra/db/Migrator.h"
#include "typeit/infra/db/SqliteDatabase.h"
#include "typeit/infra/db/SqliteHistoryRepository.h"

namespace typeit::infra {
    namespace {

        using core::Result;

        constexpr std::int64_t kMillisPerDay = 86'400'000;
        constexpr core::Millis kEpoch{1'767'225'600'000};

        /// ARCHITECTURE §6.5. Generous on purpose: a budget tight enough to
        /// flap on a loaded CI runner is a budget somebody disables.
        constexpr auto kBudget = std::chrono::milliseconds{200};

        constexpr std::size_t kSessions = 10'000;
        /// Thirty a run, which is what a thirty-second run produces.
        constexpr std::size_t kSamplesPerSession = 30;

        class HistoryPerfTest : public ::testing::Test {
        protected:
            void SetUp() override {
                Result<SqliteDatabase> database = SqliteDatabase::open_in_memory();
                ASSERT_TRUE(database) << (database ? "" : database.error().context);
                database_ = std::make_unique<SqliteDatabase>(std::move(*database));
                ASSERT_TRUE(migrate_to_latest(*database_));
                repository_ = std::make_unique<SqliteHistoryRepository>(*database_);
                seed();
            }

            /// Ten thousand runs across eight years, thirty samples each.
            ///
            /// Written through the repository so the rows are the ones the
            /// application actually produces — a fixture that inserted its own
            /// SQL could seed a shape the program never writes and prove
            /// nothing about it.
            void seed() {
                // No outer transaction: `save` owns its own, and SQLite does
                // not nest them — wrapping the loop makes every save fail,
                // which looks exactly like a very fast seed. The database is in
                // memory, so ten thousand commits cost no disk sync anyway.
                for (std::size_t at = 0; at < kSessions; ++at) {
                    app::SessionRecord record;
                    // Spread over days, several runs a day, so the day grouping
                    // has real work to do rather than ten thousand buckets.
                    const auto day = static_cast<std::int64_t>(at / 4);
                    record.started_at = core::Millis{kEpoch.value + (day * kMillisPerDay) +
                                                     (static_cast<std::int64_t>(at % 4) * 3'600'000)};
                    record.ended_at = record.started_at + core::Millis{30'000};
                    record.mode = (at % 3 == 0) ? "quote" : "timed";
                    record.mode_param = R"({"seconds":30})";
                    record.provider = "whole";
                    record.duration = core::Millis{30'000};
                    record.graphemes_typed = 150;
                    record.graphemes_correct = 148;
                    record.net_wpm = core::Wpm{60.0 + static_cast<double>(at % 20)};
                    record.gross_wpm = record.net_wpm;
                    record.raw_wpm = record.net_wpm;
                    record.accuracy = core::Accuracy{0.97};
                    record.final_correctness = core::Accuracy{0.99};
                    record.consistency = 85.0;
                    record.completed = true;
                    record.app_version = "test";
                    for (std::size_t second = 0; second < kSamplesPerSession; ++second) {
                        record.timeline.push_back({.at = core::Millis{static_cast<std::int64_t>(second) * 1'000},
                                                   .wpm = core::Wpm{60.0},
                                                   .keystrokes = 5,
                                                   .errors = 0,
                                                   .pacer_wpm = std::nullopt});
                    }
                    ASSERT_TRUE(repository_->save(record)) << "seeding run " << at;
                }
            }

            /// The plan SQLite would use for a statement, as text.
            [[nodiscard]] std::string plan_for(std::string_view sql) {
                Result<Statement> statement = database_->prepare(std::string{"EXPLAIN QUERY PLAN "} + std::string{sql});
                EXPECT_TRUE(statement) << (statement ? "" : statement.error().context);
                if (!statement) {
                    return {};
                }
                // Statements are cached and handed back as they were left, so
                // a plan read twice would come back empty the second time.
                statement->reset();

                std::string plan;
                for (;;) {
                    const Result<bool> row = statement->step();
                    if (!row || !*row) {
                        break;
                    }
                    plan += statement->column_text(3);
                    plan += '\n';
                }
                return plan;
            }

            std::unique_ptr<SqliteDatabase> database_;
            std::unique_ptr<SqliteHistoryRepository> repository_;
        };

        TEST_F(HistoryPerfTest, TheFixtureReallyHoldsTenThousandSessions) {
            // A budget met against an empty database is a budget met against
            // nothing, and a seeding bug would look exactly like a fast load.
            const Result<std::int64_t> sessions = database_->query_int("SELECT COUNT(*) FROM session");
            const Result<std::int64_t> samples = database_->query_int("SELECT COUNT(*) FROM session_sample");

            ASSERT_TRUE(sessions);
            ASSERT_TRUE(samples);
            EXPECT_EQ(*sessions, static_cast<std::int64_t>(kSessions));
            EXPECT_EQ(*samples, static_cast<std::int64_t>(kSessions * kSamplesPerSession));
        }

        TEST_F(HistoryPerfTest, TheHistoryScreensDataLoadsInsideTheBudget) {
            // Everything the history screen asks for, in the order it asks.
            const app::HistoryService service{*repository_};
            app::HistoryFilter filter;
            filter.completed_only = false;
            filter.limit = 200;  // What the screen pages through.

            const auto started = std::chrono::steady_clock::now();
            const Result<std::vector<app::SessionRow>> rows = repository_->query(filter);
            const Result<app::Aggregates> totals = repository_->aggregates(filter);
            const Result<std::vector<app::PersonalBest>> bests = repository_->personal_bests();
            const Result<core::KeyStats> keys = repository_->key_stats(filter);
            const Result<std::vector<app::TrendPoint>> trend = service.trend(filter, app::TrendBucket::Day);
            const Result<app::Streak> streak = service.streak(filter, kEpoch);
            const auto elapsed =
                    std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - started);

            ASSERT_TRUE(rows);
            ASSERT_TRUE(totals);
            ASSERT_TRUE(bests);
            ASSERT_TRUE(keys);
            ASSERT_TRUE(trend);
            ASSERT_TRUE(streak);
            EXPECT_LE(elapsed, kBudget) << "took " << elapsed.count() << " ms over " << kSessions << " sessions";
        }

        TEST_F(HistoryPerfTest, TheTrendIsAggregatedBySqlRatherThanInMemory) {
            // The load above could be fast today and quadratic tomorrow. This
            // is the property that keeps it: the day grouping is a scan of an
            // index, not a read of every row into a vector.
            // The statement the repository actually runs, not a paraphrase.
            // The first draft of this test planned a hand-copied
            // simplification, reported a table scan the real query does not do,
            // and would have gone on passing after the real one regressed.
            const std::string plan = plan_for(SqliteHistoryRepository::daily_totals_sql());

            EXPECT_NE(plan.find("INDEX"), std::string::npos) << "expected an index scan, got:\n" << plan;
            EXPECT_EQ(plan.find("SCAN session\n"), std::string::npos) << "a bare table scan:\n" << plan;
        }

        TEST_F(HistoryPerfTest, TheSessionListUsesTheStartedAtIndexRatherThanSortingEveryRow) {
            const std::string plan = plan_for(
                    "SELECT id, started_at, mode FROM session WHERE (?1 = '' OR mode = ?1)"
                    " ORDER BY started_at DESC, id DESC LIMIT 50");

            EXPECT_NE(plan.find("idx_session"), std::string::npos) << plan;
        }

        TEST_F(HistoryPerfTest, TheTrendDoesNotGrowWithTheHistoryBehindIt) {
            // The memory assertion, made about a count rather than about bytes:
            // a year of practice is at most 366 buckets whether there are four
            // runs behind each of them or four hundred. Loading rows and
            // grouping them in C++ made this number the *session* count.
            const app::HistoryService service{*repository_};
            app::HistoryFilter filter;
            filter.completed_only = false;

            const Result<std::vector<app::TrendPoint>> trend = service.trend(filter, app::TrendBucket::Day);

            ASSERT_TRUE(trend);
            EXPECT_EQ(trend->size(), kSessions / 4) << "one point per day typed, not one per run";
            EXPECT_LT(trend->size(), kSessions);
        }

    }  // namespace
}  // namespace typeit::infra
