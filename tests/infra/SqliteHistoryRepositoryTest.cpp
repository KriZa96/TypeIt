#include <algorithm>
#include <cstdint>
#include <gtest/gtest.h>
#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "typeit/app/records/History.h"
#include "typeit/core/metrics/ErrorMap.h"
#include "typeit/core/metrics/KeyStats.h"
#include "typeit/core/metrics/Timeline.h"
#include "typeit/core/util/Result.h"
#include "typeit/core/util/Units.h"
#include "typeit/infra/db/Migrator.h"
#include "typeit/infra/db/SqliteDatabase.h"
#include "typeit/infra/db/SqliteHistoryRepository.h"

namespace typeit::infra {
    namespace {

        using core::Result;
        using core::Status;

        constexpr std::int64_t kMillisPerDay = 86'400'000;
        constexpr core::Millis kNoon{1'767'225'600'000};  // 2026-01-01, near enough

        /// A completed run, good enough to set a record, with everything a
        /// caller would fill in. Tests change the one field they are about.
        app::SessionRecord a_run() {
            // Assigned rather than aggregate-initialised: a designated
            // initialiser that leaves the optional fields alone is exactly what
            // -Wmissing-field-initializers complains about, and listing every
            // field at every call site would bury the one the test is about.
            app::SessionRecord record;
            record.started_at = kNoon;
            record.ended_at = kNoon + core::Millis{30'000};
            record.mode = "timed";
            record.mode_param = R"({"seconds":30})";
            record.provider = "whole";
            record.provider_seed = 4242;
            record.duration = core::Millis{30'000};
            record.graphemes_typed = 300;
            record.graphemes_correct = 295;
            record.errors_total = 5;
            record.errors_uncorrected = 5;
            record.backspaces = 2;
            record.raw_wpm = core::Wpm{120.0};
            record.gross_wpm = core::Wpm{118.0};
            record.net_wpm = core::Wpm{108.0};
            record.accuracy = core::Accuracy{0.98};
            record.final_correctness = core::Accuracy{0.983};
            record.consistency = 92.5;
            record.completed = true;
            record.app_version = "2.0.0-alpha.3";
            return record;
        }

        core::TimelineSample a_sample(std::int64_t at, double wpm, std::size_t errors) {
            core::TimelineSample sample;
            sample.at = core::Millis{at};
            sample.wpm = core::Wpm{wpm};
            sample.keystrokes = 10;
            sample.errors = errors;
            return sample;
        }

        core::KeyStat a_key_stat(std::size_t attempts, std::size_t errors, std::int64_t latency) {
            core::KeyStat stat;
            stat.attempts = attempts;
            stat.errors = errors;
            stat.total_latency = core::Millis{latency};
            stat.latency_samples = attempts;
            return stat;
        }

        app::HistoryFilter only(std::string mode) {
            app::HistoryFilter filter;
            filter.mode = std::move(mode);
            return filter;
        }

        app::HistoryFilter between(core::Millis since, core::Millis until) {
            app::HistoryFilter filter;
            filter.since = since;
            filter.until = until;
            return filter;
        }

        app::HistoryFilter first(std::size_t limit) {
            app::HistoryFilter filter;
            filter.limit = limit;
            return filter;
        }

        app::HistoryFilter including_abandoned() {
            app::HistoryFilter filter;
            filter.completed_only = false;
            return filter;
        }

        class HistoryTest : public ::testing::Test {
        protected:
            void SetUp() override {
                Result<SqliteDatabase> database = SqliteDatabase::open_in_memory();
                ASSERT_TRUE(database) << (database ? "" : database.error().context);
                database_ = std::make_unique<SqliteDatabase>(std::move(*database));
                ASSERT_TRUE(migrate_to_latest(*database_));
                repository_ = std::make_unique<SqliteHistoryRepository>(*database_);
            }

            [[nodiscard]] std::int64_t count(std::string_view sql) {
                const Result<std::int64_t> value = database_->query_int(sql);
                EXPECT_TRUE(value) << (value ? "" : value.error().context);
                return value.value_or(-1);
            }

            std::unique_ptr<SqliteDatabase> database_;
            std::unique_ptr<SqliteHistoryRepository> repository_;
        };

        // ---- saving --------------------------------------------------------

        TEST_F(HistoryTest, SavingWritesOneSessionAndItsSamples) {
            app::SessionRecord record = a_run();
            record.timeline = {a_sample(0, 110.0, 0), a_sample(1'000, 120.0, 1)};

            const Result<core::SessionId> id = repository_->save(record);

            ASSERT_TRUE(id) << (id ? "" : id.error().context);
            EXPECT_GT(id->value, 0);
            EXPECT_EQ(count("SELECT COUNT(*) FROM session"), 1);
            EXPECT_EQ(count("SELECT COUNT(*) FROM session_sample"), 2);
        }

        TEST_F(HistoryTest, EveryFieldSurvivesTheRoundTrip) {
            const app::SessionRecord record = a_run();
            ASSERT_TRUE(repository_->save(record));

            const Result<std::vector<app::SessionRow>> rows = repository_->query({});

            ASSERT_TRUE(rows) << (rows ? "" : rows.error().context);
            ASSERT_EQ(rows->size(), 1U);
            const app::SessionRow& row = rows->front();
            EXPECT_EQ(row.started_at, record.started_at);
            EXPECT_EQ(row.mode, "timed");
            EXPECT_EQ(row.mode_param, R"({"seconds":30})");
            EXPECT_EQ(row.duration, core::Millis{30'000});
            EXPECT_DOUBLE_EQ(row.net_wpm.value, 108.0);
            EXPECT_DOUBLE_EQ(row.accuracy.value, 0.98);
            EXPECT_DOUBLE_EQ(row.consistency, 92.5);
            EXPECT_TRUE(row.completed);
        }

        TEST_F(HistoryTest, AFailureMidSaveLeavesTheDatabaseUntouched) {
            // A sample that violates the primary key: two rows at the same
            // millisecond of the same session. The session insert already
            // succeeded, so only the transaction can undo it.
            app::SessionRecord record = a_run();
            record.timeline = {a_sample(0, 110.0, 0), a_sample(0, 111.0, 0)};

            const Result<core::SessionId> id = repository_->save(record);

            ASSERT_FALSE(id);
            EXPECT_EQ(count("SELECT COUNT(*) FROM session"), 0) << "a run is saved whole or not at all";
            EXPECT_EQ(count("SELECT COUNT(*) FROM session_sample"), 0);
            EXPECT_EQ(count("SELECT COUNT(*) FROM personal_best"), 0);
            EXPECT_FALSE(database_->in_transaction());
        }

        TEST_F(HistoryTest, AnAbsentTextIsStoredAsNull) {
            const app::SessionRecord record = a_run();
            ASSERT_FALSE(record.text_id.has_value());

            ASSERT_TRUE(repository_->save(record));

            EXPECT_EQ(count("SELECT COUNT(*) FROM session WHERE text_id IS NULL"), 1);
        }

        // ---- querying ------------------------------------------------------

        TEST_F(HistoryTest, QueryHonoursTheModeFilter) {
            app::SessionRecord timed = a_run();
            app::SessionRecord quote = a_run();
            quote.mode = "quote";
            ASSERT_TRUE(repository_->save(timed));
            ASSERT_TRUE(repository_->save(quote));

            const Result<std::vector<app::SessionRow>> rows = repository_->query(only("quote"));

            ASSERT_TRUE(rows);
            ASSERT_EQ(rows->size(), 1U);
            EXPECT_EQ(rows->front().mode, "quote");
        }

        TEST_F(HistoryTest, QueryHonoursTheDateRangeAtItsExactBoundaries) {
            // Half-open, like the timeline buckets: a run on the lower bound is
            // in, a run on the upper bound is not.
            for (const std::int64_t offset: {0, 1'000, 2'000}) {
                app::SessionRecord record = a_run();
                record.started_at = kNoon + core::Millis{offset};
                ASSERT_TRUE(repository_->save(record));
            }

            const Result<std::vector<app::SessionRow>> rows =
                    repository_->query(between(kNoon + core::Millis{1'000}, kNoon + core::Millis{2'000}));

            ASSERT_TRUE(rows);
            ASSERT_EQ(rows->size(), 1U);
            EXPECT_EQ(rows->front().started_at, kNoon + core::Millis{1'000});
        }

        TEST_F(HistoryTest, QueryHonoursTheLimitAndTheOrdering) {
            for (const std::int64_t offset: {0, 1'000, 2'000}) {
                app::SessionRecord record = a_run();
                record.started_at = kNoon + core::Millis{offset};
                ASSERT_TRUE(repository_->save(record));
            }

            const Result<std::vector<app::SessionRow>> rows = repository_->query(first(2));

            ASSERT_TRUE(rows);
            ASSERT_EQ(rows->size(), 2U);
            EXPECT_EQ(rows->at(0).started_at, kNoon + core::Millis{2'000}) << "newest first";
            EXPECT_EQ(rows->at(1).started_at, kNoon + core::Millis{1'000});
        }

        TEST_F(HistoryTest, AbandonedRunsAreExcludedUnlessAskedFor) {
            app::SessionRecord abandoned = a_run();
            abandoned.completed = false;
            ASSERT_TRUE(repository_->save(a_run()));
            ASSERT_TRUE(repository_->save(abandoned));

            EXPECT_EQ(repository_->query({}).value_or(std::vector<app::SessionRow>{}).size(), 1U);
            EXPECT_EQ(repository_->query(including_abandoned()).value_or(std::vector<app::SessionRow>{}).size(), 2U);
        }

        TEST_F(HistoryTest, AnEmptyDatabaseQueriesToNothing) {
            const Result<std::vector<app::SessionRow>> rows = repository_->query({});

            ASSERT_TRUE(rows) << (rows ? "" : rows.error().context);
            EXPECT_TRUE(rows->empty());
        }

        // ---- aggregates ----------------------------------------------------

        TEST_F(HistoryTest, AggregatesComputeOverTheFilteredRange) {
            for (const double net: {60.0, 90.0, 120.0}) {
                app::SessionRecord record = a_run();
                record.net_wpm = core::Wpm{net};
                ASSERT_TRUE(repository_->save(record));
            }

            const Result<app::Aggregates> totals = repository_->aggregates({});

            ASSERT_TRUE(totals) << (totals ? "" : totals.error().context);
            EXPECT_EQ(totals->sessions, 3U);
            EXPECT_DOUBLE_EQ(totals->mean_net_wpm.value, 90.0);
            EXPECT_DOUBLE_EQ(totals->best_net_wpm.value, 120.0);
            EXPECT_DOUBLE_EQ(totals->worst_net_wpm.value, 60.0);
            EXPECT_EQ(totals->total_time, core::Millis{90'000});
            EXPECT_EQ(totals->total_graphemes, 900U);
        }

        TEST_F(HistoryTest, AggregatesOverAnEmptyRangeAreZerosNotNaN) {
            // A new user's history screen is a normal thing to draw.
            const Result<app::Aggregates> totals = repository_->aggregates({});

            ASSERT_TRUE(totals) << (totals ? "" : totals.error().context);
            EXPECT_EQ(totals->sessions, 0U);
            EXPECT_DOUBLE_EQ(totals->mean_net_wpm.value, 0.0);
            EXPECT_DOUBLE_EQ(totals->mean_accuracy.value, 0.0);
            EXPECT_EQ(totals->total_time, core::Millis{0});
        }

        // ---- personal bests ------------------------------------------------

        TEST_F(HistoryTest, ABetterRunReplacesTheRecord) {
            app::SessionRecord first = a_run();
            first.net_wpm = core::Wpm{90.0};
            ASSERT_TRUE(repository_->save(first));
            app::SessionRecord better = a_run();
            better.net_wpm = core::Wpm{110.0};
            const Result<core::SessionId> best_id = repository_->save(better);
            ASSERT_TRUE(best_id);

            const Result<std::vector<app::PersonalBest>> bests = repository_->personal_bests();

            ASSERT_TRUE(bests);
            const auto net = std::ranges::find_if(
                    *bests, [](const app::PersonalBest& best) { return best.metric == "net_wpm"; });
            ASSERT_NE(net, bests->end());
            EXPECT_DOUBLE_EQ(net->value, 110.0);
            EXPECT_EQ(net->session_id, *best_id);
        }

        TEST_F(HistoryTest, AWorseRunDoesNotReplaceTheRecord) {
            app::SessionRecord best = a_run();
            best.net_wpm = core::Wpm{110.0};
            ASSERT_TRUE(repository_->save(best));
            app::SessionRecord worse = a_run();
            worse.net_wpm = core::Wpm{90.0};
            ASSERT_TRUE(repository_->save(worse));

            const Result<std::vector<app::PersonalBest>> bests = repository_->personal_bests();

            ASSERT_TRUE(bests);
            const auto net = std::ranges::find_if(
                    *bests, [](const app::PersonalBest& best_row) { return best_row.metric == "net_wpm"; });
            ASSERT_NE(net, bests->end());
            EXPECT_DOUBLE_EQ(net->value, 110.0);
        }

        TEST_F(HistoryTest, AnEqualRunLeavesTheOlderRecordStanding) {
            // The documented tie-break: the first person there keeps it, and
            // the achieved_at date does not churn.
            const Result<core::SessionId> first = repository_->save(a_run());
            ASSERT_TRUE(first);
            ASSERT_TRUE(repository_->save(a_run()));

            const Result<std::vector<app::PersonalBest>> bests = repository_->personal_bests();

            ASSERT_TRUE(bests);
            const auto net = std::ranges::find_if(
                    *bests, [](const app::PersonalBest& best) { return best.metric == "net_wpm"; });
            ASSERT_NE(net, bests->end());
            EXPECT_EQ(net->session_id, *first);
        }

        TEST_F(HistoryTest, AnAbandonedRunNeverSetsARecord) {
            app::SessionRecord abandoned = a_run();
            abandoned.completed = false;
            abandoned.net_wpm = core::Wpm{999.0};

            ASSERT_TRUE(repository_->save(abandoned));

            EXPECT_EQ(count("SELECT COUNT(*) FROM personal_best"), 0);
        }

        TEST_F(HistoryTest, ARunBelowTheAccuracyFloorNeverSetsARecord) {
            // A personal best cannot be bought by typing nonsense quickly.
            app::SessionRecord sloppy = a_run();
            sloppy.accuracy = core::Accuracy{0.89};
            sloppy.net_wpm = core::Wpm{999.0};

            ASSERT_TRUE(repository_->save(sloppy));

            EXPECT_EQ(count("SELECT COUNT(*) FROM personal_best"), 0);
        }

        TEST_F(HistoryTest, RecordsAreKeptPerModeAndParameter) {
            // A 15-second best and a 60-second best measure different things.
            app::SessionRecord short_run = a_run();
            short_run.mode_param = R"({"seconds":15})";
            app::SessionRecord long_run = a_run();
            long_run.mode_param = R"({"seconds":60})";
            ASSERT_TRUE(repository_->save(short_run));
            ASSERT_TRUE(repository_->save(long_run));

            EXPECT_EQ(count("SELECT COUNT(*) FROM personal_best WHERE metric = 'net_wpm'"), 2);
        }

        // ---- key statistics ------------------------------------------------

        TEST_F(HistoryTest, KeyStatsMergeRatherThanReplace) {
            core::KeyStats stats;
            stats.per_grapheme["č"] = a_key_stat(10, 2, 1'000);
            stats.per_bigram["ča"] = a_key_stat(4, 1, 400);

            ASSERT_TRUE(repository_->merge_key_stats(stats));
            ASSERT_TRUE(repository_->merge_key_stats(stats));

            const Result<core::KeyStats> merged = repository_->key_stats({});
            ASSERT_TRUE(merged) << (merged ? "" : merged.error().context);
            EXPECT_EQ(merged->per_grapheme.at("č").attempts, 20U) << "merging twice doubles the counts";
            EXPECT_EQ(merged->per_grapheme.at("č").errors, 4U);
            EXPECT_EQ(merged->per_grapheme.at("č").total_latency, core::Millis{2'000});
            EXPECT_EQ(merged->per_bigram.at("ča").attempts, 8U);
        }

        TEST_F(HistoryTest, ANewGraphemeIsInsertedRatherThanMerged) {
            core::KeyStats one;
            one.per_grapheme["a"] = a_key_stat(1, 0, 100);
            core::KeyStats other;
            other.per_grapheme["👍"] = a_key_stat(3, 1, 300);

            ASSERT_TRUE(repository_->merge_key_stats(one));
            ASSERT_TRUE(repository_->merge_key_stats(other));

            const Result<core::KeyStats> merged = repository_->key_stats({});
            ASSERT_TRUE(merged);
            EXPECT_EQ(merged->per_grapheme.size(), 2U);
            EXPECT_EQ(merged->per_grapheme.at("👍").attempts, 3U) << "and unicode round-trips";
        }

        TEST_F(HistoryTest, AnEmptyMergeIsNotAFailure) {
            EXPECT_TRUE(repository_->merge_key_stats({}));
            EXPECT_TRUE(repository_->merge_error_map({}));
        }

        TEST_F(HistoryTest, ErrorPairsMerge) {
            core::ErrorMap errors;
            errors.substitutions[{"m", "n"}] = 3;

            ASSERT_TRUE(repository_->merge_error_map(errors));
            ASSERT_TRUE(repository_->merge_error_map(errors));

            EXPECT_EQ(count("SELECT count FROM error_pair WHERE expected = 'm' AND typed = 'n'"), 6);
        }

        // ---- best sustained ------------------------------------------------

        TEST_F(HistoryTest, BestSustainedIgnoresRunsOutsideTheWindow) {
            const Result<std::int64_t> now =
                    database_->query_int("SELECT CAST(strftime('%s', 'now') AS INTEGER) * 1000");
            ASSERT_TRUE(now);

            app::SessionRecord recent = a_run();
            recent.started_at = core::Millis{*now - (5 * kMillisPerDay)};
            recent.peak_wpm = core::Wpm{80.0};
            app::SessionRecord ancient = a_run();
            ancient.started_at = core::Millis{*now - (100 * kMillisPerDay)};
            ancient.peak_wpm = core::Wpm{200.0};
            ASSERT_TRUE(repository_->save(recent));
            ASSERT_TRUE(repository_->save(ancient));

            const Result<core::Wpm> best = repository_->best_sustained_wpm(core::Days{30});

            ASSERT_TRUE(best) << (best ? "" : best.error().context);
            EXPECT_DOUBLE_EQ(best->value, 80.0) << "the 100-day-old run is outside the window";
        }

        TEST_F(HistoryTest, BestSustainedWithNoHistoryIsZero) {
            const Result<core::Wpm> best = repository_->best_sustained_wpm(core::Days{30});

            ASSERT_TRUE(best) << (best ? "" : best.error().context);
            EXPECT_DOUBLE_EQ(best->value, 0.0) << "not an error: a new user has no races behind them";
        }

        TEST_F(HistoryTest, BestSustainedIgnoresRunsWithoutAPacer) {
            // Only races have a peak; a timed run has nothing to report here.
            ASSERT_TRUE(repository_->save(a_run()));

            const Result<core::Wpm> best = repository_->best_sustained_wpm(core::Days{30});

            ASSERT_TRUE(best);
            EXPECT_DOUBLE_EQ(best->value, 0.0);
        }

    }  // namespace
}  // namespace typeit::infra
