// Where progression actually lives (TI-126, GAMEPLAY section 3.4).
//
//     V₀ = max(V_floor, α · best_sustained_wpm over the last 30 days)
//
// Today's ceiling becomes tomorrow's floor. Without this the ramp starts from
// the same place every session and the typist re-grinds speeds they have
// already proven, which is the difference between a game that trains somebody
// and a game that entertains them for an evening.
//
// The formula itself is `ProfileServiceTest`'s. What is here is the part that
// only shows up over several sessions: that an improving typist gets a rising
// start, and that runs which should not count do not.

#include <cstdint>
#include <gtest/gtest.h>
#include <vector>

#include "typeit/app/records/History.h"
#include "typeit/app/services/ProfileService.h"
#include "typeit/core/config/Config.h"
#include "typeit/core/util/Result.h"
#include "typeit/core/util/Units.h"
#include "typeit/testing/Fakes.h"

namespace typeit::app {
    namespace {

        constexpr core::Millis kNow{1'767'225'600'000};
        constexpr std::int64_t kDay = 86'400'000;

        class AdaptiveStartTest : public ::testing::Test {
        protected:
            void SetUp() override { history.now = kNow; }

            /// A finished race that held `peak` for long enough to count.
            void a_race(double peak, double accuracy = 0.97, std::int64_t days_ago = 0) {
                SessionRecord record;
                record.mode = "race";
                record.started_at = core::Millis{kNow.value - (days_ago * kDay)};
                record.completed = true;
                record.accuracy = core::Accuracy{accuracy};
                record.peak_wpm = core::Wpm{peak};
                history.records.push_back(record);
            }

            [[nodiscard]] double start() {
                const core::Result<core::Wpm> speed = ProfileService{history}.starting_speed();
                EXPECT_TRUE(speed) << (speed ? "" : speed.error().context);
                return speed.value_or(core::Wpm{-1.0}).value;
            }

            testing::FakeHistoryRepository history;
        };

        // ---- the formula, end to end -------------------------------------------------

        TEST_F(AdaptiveStartTest, AFirstEverRaceStartsAtTheFloor) {
            EXPECT_DOUBLE_EQ(start(), kDefaultStartFloor.value);
        }

        TEST_F(AdaptiveStartTest, ANinetyWordSustainedBestStartsTheNextRaceAtSeventySixAndAHalf) {
            a_race(90.0);

            EXPECT_DOUBLE_EQ(start(), 76.5);
        }

        TEST_F(AdaptiveStartTest, HistoryOlderThanThirtyDaysDoesNotCount) {
            // A speed held six weeks ago is not a speed anybody has now.
            a_race(120.0, 0.97, 31);
            a_race(40.0, 0.97, 1);

            EXPECT_DOUBLE_EQ(start(), 34.0) << "0.85 of the recent 40, not of the stale 120";
        }

        // ---- what does not count ---------------------------------------------------------

        TEST_F(AdaptiveStartTest, ARunTypedBadlyDoesNotSetTheStartingSpeed) {
            // The gap this issue closes. A ramp will happily push the pacer to
            // 120 while the typist mashes at 60% accuracy; starting tomorrow
            // there would mean losing immediately, every time, forever.
            a_race(120.0, 0.60);
            a_race(50.0, 0.97);

            EXPECT_DOUBLE_EQ(start(), 42.5) << "the 50 counted and the 120 did not";
        }

        TEST_F(AdaptiveStartTest, TheAccuracyBarIsNinetyPercentAndIsClosedAtIt) {
            a_race(100.0, 0.90);

            EXPECT_DOUBLE_EQ(start(), 85.0) << "exactly ninety is a run that counts";
        }

        TEST_F(AdaptiveStartTest, AnAbandonedRaceDoesNotSetTheStartingSpeed) {
            SessionRecord quit;
            quit.mode = "race";
            quit.started_at = kNow;
            quit.completed = false;
            quit.accuracy = core::Accuracy{0.99};
            quit.peak_wpm = core::Wpm{150.0};
            history.records.push_back(quit);

            EXPECT_DOUBLE_EQ(start(), kDefaultStartFloor.value);
        }

        // ---- the progression property -----------------------------------------------------

        TEST_F(AdaptiveStartTest, AnImprovingTypistGetsAStrictlyRisingStartingSpeed) {
            // The feature's whole justification, so it is a test rather than an
            // assumption. Twelve sessions, each a little better than the last:
            // the speed the ramp begins at has to climb with them, or the
            // typist re-grinds the same ground every evening.
            std::vector<double> starts;
            for (int session = 0; session < 12; ++session) {
                // Well above the floor from the outset, so the property being
                // measured is the adaptation and not the clamp.
                a_race(60.0 + (5.0 * session));
                starts.push_back(start());
            }

            ASSERT_EQ(starts.size(), 12U);
            for (std::size_t at = 1; at < starts.size(); ++at) {
                EXPECT_GT(starts[at], starts[at - 1]) << "session " << at << " did not start above session " << at - 1;
            }
            EXPECT_DOUBLE_EQ(starts.front(), 51.0) << "0.85 of the first race's 60";
            EXPECT_DOUBLE_EQ(starts.back(), 0.85 * 115.0);
        }

        TEST_F(AdaptiveStartTest, APlateauedTypistStopsRisingRatherThanDrifting) {
            // The other half of the same property: adaptation tracks the best,
            // so repeating yesterday's performance repeats yesterday's start.
            for (int session = 0; session < 5; ++session) {
                a_race(80.0);
            }

            EXPECT_DOUBLE_EQ(start(), 68.0);
        }

        TEST_F(AdaptiveStartTest, ABadDayDoesNotUndoWeeksOfProgress) {
            // The best of the window, not the last of it. Somebody tired on a
            // Tuesday has not become a slower typist.
            a_race(100.0, 0.97, 5);
            a_race(30.0, 0.97, 0);

            EXPECT_DOUBLE_EQ(start(), 85.0);
        }

        // ---- the policy switch -------------------------------------------------------------

        TEST_F(AdaptiveStartTest, AFixedPolicyIgnoresTheHistoryEntirely) {
            a_race(200.0);
            core::Config config;
            config.race.start_policy = "fixed";
            config.race.start_wpm = 45;

            const core::Result<core::Wpm> speed = ProfileService{history}.starting_speed(config);

            ASSERT_TRUE(speed);
            EXPECT_DOUBLE_EQ(speed->value, 45.0);

            // And did not even ask, so a fixed start survives a database that
            // cannot be read: arming the failure switch changes nothing.
            history.failure.fail_next(
                    core::Error{.code = core::ErrorCode::DbQuery, .message = "unreadable", .context = {}});
            const core::Result<core::Wpm> again = ProfileService{history}.starting_speed(config);
            ASSERT_TRUE(again) << (again ? "" : again.error().context);
            EXPECT_DOUBLE_EQ(again->value, 45.0);
        }

        TEST_F(AdaptiveStartTest, TheConfiguredStartActsAsTheFloorWhenHistoryIsConsulted) {
            core::Config config;
            config.race.start_wpm = 50;

            const core::Result<core::Wpm> speed = ProfileService{history}.starting_speed(config);

            ASSERT_TRUE(speed);
            EXPECT_DOUBLE_EQ(speed->value, 50.0) << "no history, so the configured start is what is left";
        }

    }  // namespace
}  // namespace typeit::app
