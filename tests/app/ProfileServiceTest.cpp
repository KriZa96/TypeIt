// The starting speed (TI-073).

#include <cstdint>
#include <gtest/gtest.h>

#include "typeit/app/records/History.h"
#include "typeit/app/services/ProfileService.h"
#include "typeit/core/config/Config.h"
#include "typeit/core/util/Result.h"
#include "typeit/core/util/Units.h"
#include "typeit/testing/Fakes.h"

namespace typeit::app {
    namespace {

        constexpr std::int64_t kMillisPerDay = 86'400'000;
        /// An arbitrary but fixed "today", so no test depends on the day it runs.
        constexpr core::Millis kToday{1'000 * kMillisPerDay};

        class ProfileServiceTest : public ::testing::Test {
        protected:
            ProfileServiceTest() { history_.now = kToday; }

            /// A race that held `sustained` WPM, `days_ago` days ago.
            void a_race(double sustained, std::int64_t days_ago, bool completed = true) {
                SessionRecord record;
                record.mode = "race";
                record.completed = completed;
                record.accuracy = core::Accuracy{0.99};
                record.started_at = core::Millis{kToday.value - (days_ago * kMillisPerDay)};
                record.peak_wpm = core::Wpm{sustained};
                ASSERT_TRUE(history_.save(record));
            }

            [[nodiscard]] double speed() const {
                const core::Result<core::Wpm> start = service_.starting_speed();
                EXPECT_TRUE(start) << (start ? "" : start.error().context);
                return start.value_or(core::Wpm{-1.0}).value;
            }

            testing::FakeHistoryRepository history_;
            ProfileService service_{history_};
        };

        TEST_F(ProfileServiceTest, NoHistoryStartsAtTheFloor) {
            EXPECT_DOUBLE_EQ(speed(), 20.0) << "a first-ever run has nothing to be a fraction of";
        }

        TEST_F(ProfileServiceTest, TheStartIsAFractionOfTheBestSustainedSpeed) {
            a_race(100.0, 5);

            EXPECT_DOUBLE_EQ(speed(), 85.0) << "0.85 x 100";
        }

        TEST_F(ProfileServiceTest, TheFloorWinsWhenTheFractionIsBelowIt) {
            a_race(10.0, 5);

            EXPECT_DOUBLE_EQ(speed(), 20.0) << "0.85 x 10 is 8.5, and nobody starts below 20";
        }

        TEST_F(ProfileServiceTest, TheBestOfTheWindowIsWhatCounts) {
            a_race(60.0, 20);
            a_race(100.0, 10);
            a_race(80.0, 1);

            EXPECT_DOUBLE_EQ(speed(), 85.0) << "the best, not the most recent";
        }

        TEST_F(ProfileServiceTest, HistoryOlderThanThirtyDaysIsExcluded) {
            a_race(200.0, 40);
            a_race(100.0, 5);

            EXPECT_DOUBLE_EQ(speed(), 85.0) << "a speed from six weeks ago is not today's ceiling";
        }

        TEST_F(ProfileServiceTest, ARunExactlyOnTheBoundaryIsInside) {
            // The window is the last thirty days inclusive of its edge, the
            // same half-open convention the history filters use: a run exactly
            // thirty days old belongs to exactly one side, and it is this one.
            a_race(100.0, 30);

            EXPECT_DOUBLE_EQ(speed(), 85.0);
        }

        TEST_F(ProfileServiceTest, OnlyCompletedRunsContribute) {
            a_race(200.0, 5, /*completed=*/false);
            a_race(100.0, 5);

            EXPECT_DOUBLE_EQ(speed(), 85.0) << "an abandoned run is not a speed anyone held";
        }

        TEST_F(ProfileServiceTest, TheFactorAndTheFloorAreConfigurable) {
            a_race(100.0, 5);

            const core::Result<core::Wpm> half = service_.starting_speed(0.5, core::Wpm{20.0});
            const core::Result<core::Wpm> high_floor = service_.starting_speed(0.85, core::Wpm{90.0});

            ASSERT_TRUE(half);
            ASSERT_TRUE(high_floor);
            EXPECT_DOUBLE_EQ(half->value, 50.0);
            EXPECT_DOUBLE_EQ(high_floor->value, 90.0);
        }

        TEST_F(ProfileServiceTest, AFailureToReadTheHistoryIsNotAnsweredWithTheFloor) {
            history_.failure.fail_next(core::make_error(core::ErrorCode::DbQuery, "the database is locked"));

            const core::Result<core::Wpm> start = service_.starting_speed();

            ASSERT_FALSE(start) << "a player whose history could not be read has not gone back to 20 WPM";
            EXPECT_EQ(start.error().code, core::ErrorCode::DbQuery);
        }

        // ---- through the configuration --------------------------------------

        TEST_F(ProfileServiceTest, TheConfiguredStartWpmIsTheFloor) {
            a_race(100.0, 5);
            core::Config config;
            config.race.start_wpm = 90;

            const core::Result<core::Wpm> start = service_.starting_speed(config);

            ASSERT_TRUE(start);
            EXPECT_DOUBLE_EQ(start->value, 90.0);
        }

        TEST_F(ProfileServiceTest, AFixedStartPolicyNeverAsksTheHistory) {
            a_race(200.0, 1);
            history_.failure.fail_next(core::make_error(core::ErrorCode::DbQuery, "would have been asked"));
            core::Config config;
            config.race.start_policy = "fixed";
            config.race.start_wpm = 40;

            const core::Result<core::Wpm> start = service_.starting_speed(config);

            ASSERT_TRUE(start) << "a setting that turns off adaptation must not depend on the database";
            EXPECT_DOUBLE_EQ(start->value, 40.0);
        }

    }  // namespace
}  // namespace typeit::app
