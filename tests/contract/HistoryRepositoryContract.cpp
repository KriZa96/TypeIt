// One suite, every implementation of IHistoryRepository (TESTING section 4).
//
// A fake is only useful while it behaves like the thing it stands in for, and
// "behaves like" decays silently: the real adapter grows a rule, the fake does
// not, and a service test goes on passing against a world that no longer
// exists. Running both through the same expectations is the only way to know.
//
// Adding an implementation means adding a factory, not adding tests.

#include <cstdint>
#include <gtest/gtest.h>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "typeit/app/ports/IHistoryRepository.h"
#include "typeit/app/records/History.h"
#include "typeit/core/metrics/KeyStats.h"
#include "typeit/core/util/Result.h"
#include "typeit/core/util/Units.h"
#include "typeit/infra/db/Migrator.h"
#include "typeit/infra/db/SqliteDatabase.h"
#include "typeit/infra/db/SqliteHistoryRepository.h"
#include "typeit/infra/time/SystemClock.h"
#include "typeit/testing/Fakes.h"

namespace typeit {
    namespace {

        constexpr core::Millis kNoon{1'767'225'600'000};

        /// The SQLite adapter, over a private in-memory database.
        class SqliteHistoryFactory {
        public:
            SqliteHistoryFactory() {
                core::Result<infra::SqliteDatabase> database = infra::SqliteDatabase::open_in_memory();
                EXPECT_TRUE(database) << (database ? "" : database.error().context);
                database_ = std::make_unique<infra::SqliteDatabase>(std::move(*database));
                EXPECT_TRUE(infra::migrate_to_latest(*database_));
                repository_ = std::make_unique<infra::SqliteHistoryRepository>(*database_);
            }

            app::IHistoryRepository& repository() { return *repository_; }

            /// The clock the window queries are measured against. SQL asks the
            /// system; the fake is told. Both need the same answer for
            /// `best_sustained_wpm` to be comparable.
            [[nodiscard]] static core::Millis now() { return infra::unix_now(); }

        private:
            std::unique_ptr<infra::SqliteDatabase> database_;
            std::unique_ptr<infra::SqliteHistoryRepository> repository_;
        };

        class FakeHistoryFactory {
        public:
            FakeHistoryFactory() { repository_.now = infra::unix_now(); }

            app::IHistoryRepository& repository() { return repository_; }

            [[nodiscard]] static core::Millis now() { return infra::unix_now(); }

        private:
            testing::FakeHistoryRepository repository_;
        };

        template<typename FactoryType>
        class HistoryRepositoryContract : public ::testing::Test {
        protected:
            using Factory = FactoryType;

            app::IHistoryRepository& repository() { return factory_.repository(); }

            [[nodiscard]] static app::SessionRecord a_run() {
                app::SessionRecord record;
                record.started_at = kNoon;
                record.ended_at = kNoon + core::Millis{30'000};
                record.mode = "timed";
                record.mode_param = R"({"seconds":30})";
                record.provider = "whole";
                record.provider_seed = 7;
                record.duration = core::Millis{30'000};
                record.graphemes_typed = 300;
                record.graphemes_correct = 295;
                record.net_wpm = core::Wpm{100.0};
                record.gross_wpm = core::Wpm{110.0};
                record.raw_wpm = core::Wpm{112.0};
                record.accuracy = core::Accuracy{0.98};
                record.final_correctness = core::Accuracy{0.99};
                record.consistency = 90.0;
                record.completed = true;
                record.app_version = "test";
                return record;
            }

            FactoryType factory_;
        };

        using HistoryImplementations = ::testing::Types<SqliteHistoryFactory, FakeHistoryFactory>;

        class ImplementationNames {
        public:
            template<typename Factory>
            static std::string GetName(int index) {
                if constexpr (std::is_same_v<Factory, SqliteHistoryFactory>) {
                    return "Sqlite";
                } else {
                    return "Fake";
                }
                return std::to_string(index);
            }
        };

        TYPED_TEST_SUITE(HistoryRepositoryContract, HistoryImplementations, ImplementationNames);

        TYPED_TEST(HistoryRepositoryContract, AnEmptyRepositoryAnswersEmptyRatherThanFailing) {
            const core::Result<std::vector<app::SessionRow>> rows = this->repository().query({});
            const core::Result<app::Aggregates> totals = this->repository().aggregates({});
            const core::Result<std::vector<app::PersonalBest>> bests = this->repository().personal_bests();

            ASSERT_TRUE(rows) << (rows ? "" : rows.error().context);
            ASSERT_TRUE(totals) << (totals ? "" : totals.error().context);
            ASSERT_TRUE(bests) << (bests ? "" : bests.error().context);
            EXPECT_TRUE(rows->empty());
            EXPECT_EQ(totals->sessions, 0U);
            EXPECT_EQ(totals->mean_net_wpm.value, 0.0) << "zeros, never NaN";
            EXPECT_TRUE(bests->empty());
        }

        TYPED_TEST(HistoryRepositoryContract, ASavedRunComesBack) {
            const core::Result<core::SessionId> id = this->repository().save(TestFixture::a_run());
            ASSERT_TRUE(id) << (id ? "" : id.error().context);

            const core::Result<std::vector<app::SessionRow>> rows = this->repository().query({});

            ASSERT_TRUE(rows);
            ASSERT_EQ(rows->size(), 1U);
            EXPECT_EQ(rows->front().id, *id);
            EXPECT_EQ(rows->front().mode, "timed");
            EXPECT_DOUBLE_EQ(rows->front().net_wpm.value, 100.0);
        }

        TYPED_TEST(HistoryRepositoryContract, IdsAreDistinct) {
            const core::Result<core::SessionId> first = this->repository().save(TestFixture::a_run());
            const core::Result<core::SessionId> second = this->repository().save(TestFixture::a_run());

            ASSERT_TRUE(first);
            ASSERT_TRUE(second);
            EXPECT_NE(first->value, second->value);
        }

        TYPED_TEST(HistoryRepositoryContract, TheModeFilterSelects) {
            app::SessionRecord quote = TestFixture::a_run();
            quote.mode = "quote";
            ASSERT_TRUE(this->repository().save(TestFixture::a_run()));
            ASSERT_TRUE(this->repository().save(quote));

            app::HistoryFilter filter;
            filter.mode = "quote";
            const core::Result<std::vector<app::SessionRow>> rows = this->repository().query(filter);

            ASSERT_TRUE(rows);
            ASSERT_EQ(rows->size(), 1U);
            EXPECT_EQ(rows->front().mode, "quote");
        }

        TYPED_TEST(HistoryRepositoryContract, TheDateRangeIsHalfOpen) {
            for (const std::int64_t offset: {0, 1'000, 2'000}) {
                app::SessionRecord record = TestFixture::a_run();
                record.started_at = kNoon + core::Millis{offset};
                ASSERT_TRUE(this->repository().save(record));
            }

            app::HistoryFilter filter;
            filter.since = kNoon + core::Millis{1'000};
            filter.until = kNoon + core::Millis{2'000};
            const core::Result<std::vector<app::SessionRow>> rows = this->repository().query(filter);

            ASSERT_TRUE(rows);
            ASSERT_EQ(rows->size(), 1U) << "the lower bound is in, the upper is out";
            EXPECT_EQ(rows->front().started_at, kNoon + core::Millis{1'000});
        }

        TYPED_TEST(HistoryRepositoryContract, RowsComeBackNewestFirstAndTheLimitApplies) {
            for (const std::int64_t offset: {0, 1'000, 2'000}) {
                app::SessionRecord record = TestFixture::a_run();
                record.started_at = kNoon + core::Millis{offset};
                ASSERT_TRUE(this->repository().save(record));
            }

            app::HistoryFilter filter;
            filter.limit = 2;
            const core::Result<std::vector<app::SessionRow>> rows = this->repository().query(filter);

            ASSERT_TRUE(rows);
            ASSERT_EQ(rows->size(), 2U);
            EXPECT_EQ(rows->at(0).started_at, kNoon + core::Millis{2'000});
            EXPECT_EQ(rows->at(1).started_at, kNoon + core::Millis{1'000});
        }

        TYPED_TEST(HistoryRepositoryContract, AbandonedRunsAreExcludedUnlessAskedFor) {
            app::SessionRecord abandoned = TestFixture::a_run();
            abandoned.completed = false;
            ASSERT_TRUE(this->repository().save(TestFixture::a_run()));
            ASSERT_TRUE(this->repository().save(abandoned));

            app::HistoryFilter everything;
            everything.completed_only = false;

            EXPECT_EQ(this->repository().query({}).value_or(std::vector<app::SessionRow>{}).size(), 1U);
            EXPECT_EQ(this->repository().query(everything).value_or(std::vector<app::SessionRow>{}).size(), 2U);
        }

        TYPED_TEST(HistoryRepositoryContract, AggregatesAgree) {
            for (const double net: {60.0, 90.0, 120.0}) {
                app::SessionRecord record = TestFixture::a_run();
                record.net_wpm = core::Wpm{net};
                ASSERT_TRUE(this->repository().save(record));
            }

            const core::Result<app::Aggregates> totals = this->repository().aggregates({});

            ASSERT_TRUE(totals) << (totals ? "" : totals.error().context);
            EXPECT_EQ(totals->sessions, 3U);
            EXPECT_DOUBLE_EQ(totals->mean_net_wpm.value, 90.0);
            EXPECT_DOUBLE_EQ(totals->best_net_wpm.value, 120.0);
            EXPECT_DOUBLE_EQ(totals->worst_net_wpm.value, 60.0);
            EXPECT_EQ(totals->total_time, core::Millis{90'000});
            EXPECT_EQ(totals->total_graphemes, 900U);
        }

        TYPED_TEST(HistoryRepositoryContract, ABetterRunTakesTheRecordAndAWorseOneDoesNot) {
            app::SessionRecord first = TestFixture::a_run();
            first.net_wpm = core::Wpm{90.0};
            ASSERT_TRUE(this->repository().save(first));

            app::SessionRecord better = TestFixture::a_run();
            better.net_wpm = core::Wpm{120.0};
            const core::Result<core::SessionId> best = this->repository().save(better);
            ASSERT_TRUE(best);

            app::SessionRecord worse = TestFixture::a_run();
            worse.net_wpm = core::Wpm{70.0};
            ASSERT_TRUE(this->repository().save(worse));

            const core::Result<std::vector<app::PersonalBest>> bests = this->repository().personal_bests();
            ASSERT_TRUE(bests);
            const auto net =
                    std::ranges::find_if(*bests, [](const app::PersonalBest& row) { return row.metric == "net_wpm"; });
            ASSERT_NE(net, bests->end());
            EXPECT_DOUBLE_EQ(net->value, 120.0);
            EXPECT_EQ(net->session_id, *best);
        }

        TYPED_TEST(HistoryRepositoryContract, AnEqualRunLeavesTheOlderRecordStanding) {
            const core::Result<core::SessionId> first = this->repository().save(TestFixture::a_run());
            ASSERT_TRUE(first);
            ASSERT_TRUE(this->repository().save(TestFixture::a_run()));

            const core::Result<std::vector<app::PersonalBest>> bests = this->repository().personal_bests();
            ASSERT_TRUE(bests);
            const auto net =
                    std::ranges::find_if(*bests, [](const app::PersonalBest& row) { return row.metric == "net_wpm"; });
            ASSERT_NE(net, bests->end());
            EXPECT_EQ(net->session_id, *first);
        }

        TYPED_TEST(HistoryRepositoryContract, AnAbandonedOrInaccurateRunNeverSetsARecord) {
            app::SessionRecord abandoned = TestFixture::a_run();
            abandoned.completed = false;
            abandoned.net_wpm = core::Wpm{999.0};
            app::SessionRecord sloppy = TestFixture::a_run();
            sloppy.accuracy = core::Accuracy{0.5};
            sloppy.net_wpm = core::Wpm{999.0};

            ASSERT_TRUE(this->repository().save(abandoned));
            ASSERT_TRUE(this->repository().save(sloppy));

            const core::Result<std::vector<app::PersonalBest>> bests = this->repository().personal_bests();
            ASSERT_TRUE(bests);
            EXPECT_TRUE(bests->empty()) << "a record cannot be bought by typing nonsense quickly";
        }

        TYPED_TEST(HistoryRepositoryContract, KeyStatsMergeRatherThanReplace) {
            core::KeyStats stats;
            core::KeyStat one;
            one.attempts = 10;
            one.errors = 2;
            one.total_latency = core::Millis{1'000};
            one.latency_samples = 10;
            stats.per_grapheme["č"] = one;
            stats.per_bigram["ča"] = one;

            ASSERT_TRUE(this->repository().merge_key_stats(stats));
            ASSERT_TRUE(this->repository().merge_key_stats(stats));

            const core::Result<core::KeyStats> merged = this->repository().key_stats({});
            ASSERT_TRUE(merged) << (merged ? "" : merged.error().context);
            EXPECT_EQ(merged->per_grapheme.at("č").attempts, 20U);
            EXPECT_EQ(merged->per_grapheme.at("č").errors, 4U);
            EXPECT_EQ(merged->per_grapheme.at("č").total_latency, core::Millis{2'000});
            EXPECT_EQ(merged->per_bigram.at("ča").attempts, 20U);
        }

        TYPED_TEST(HistoryRepositoryContract, BestSustainedRespectsItsWindow) {
            const core::Millis now = TypeParam::now();
            constexpr std::int64_t kDay = 86'400'000;

            app::SessionRecord recent = TestFixture::a_run();
            recent.started_at = core::Millis{now.value - (5 * kDay)};
            recent.peak_wpm = core::Wpm{80.0};
            app::SessionRecord ancient = TestFixture::a_run();
            ancient.started_at = core::Millis{now.value - (100 * kDay)};
            ancient.peak_wpm = core::Wpm{200.0};
            ASSERT_TRUE(this->repository().save(recent));
            ASSERT_TRUE(this->repository().save(ancient));

            const core::Result<core::Wpm> best = this->repository().best_sustained_wpm(core::Days{30});

            ASSERT_TRUE(best) << (best ? "" : best.error().context);
            EXPECT_DOUBLE_EQ(best->value, 80.0);
        }

        TYPED_TEST(HistoryRepositoryContract, BestSustainedWithNoRacesIsZero) {
            ASSERT_TRUE(this->repository().save(TestFixture::a_run()));

            const core::Result<core::Wpm> best = this->repository().best_sustained_wpm(core::Days{30});

            ASSERT_TRUE(best) << (best ? "" : best.error().context);
            EXPECT_DOUBLE_EQ(best->value, 0.0) << "a timed run has no pacer to have kept up with";
        }

    }  // namespace
}  // namespace typeit
