#include <gtest/gtest.h>
#include <string_view>
#include <variant>

#include "typeit/core/modes/IMode.h"
#include "typeit/core/modes/TimedMode.h"
#include "typeit/core/util/Units.h"
#include "typeit/testing/ModeDriver.h"
#include "typeit/testing/Preconditions.h"

namespace typeit::core {
    namespace {

        constexpr std::string_view kText = "the quick brown fox jumps over the lazy dog";

        TimedProgress progress_of(const TimedMode& mode) { return std::get<TimedProgress>(mode.progress()); }

        TEST(TimedModeTest, IsRegisteredAndPersistedAsTimed) {
            const TimedMode mode{Millis{30'000}};

            EXPECT_EQ(mode.id(), "timed");
        }

        TEST(TimedModeTest, TheClockStartsOnTheFirstKeystrokeNotOnEnteringTheScreen) {
            // The behaviour 1.0 gets wrong: it starts counting when a session
            // flag flips, charging the typist for reading the text.
            TimedMode mode{Millis{10'000}};
            testing::ModeDriver driver{kText, mode};

            driver.tick(20, Millis{1'000});

            EXPECT_FALSE(mode.is_finished()) << "twenty seconds of staring at a ten-second run";
            EXPECT_EQ(progress_of(mode).elapsed, Millis{0});
            EXPECT_EQ(progress_of(mode).remaining, Millis{10'000});
        }

        TEST(TimedModeTest, TimeToTheFirstKeystrokeIsRecordedAndExcluded) {
            TimedMode mode{Millis{10'000}};
            testing::ModeDriver driver{kText, mode};

            driver.tick(4, Millis{1'000});
            driver.type("t");

            EXPECT_EQ(mode.time_to_first_keystroke(), Millis{4'100});
            EXPECT_EQ(progress_of(mode).elapsed, Millis{0}) << "the run starts now, not four seconds ago";
            EXPECT_EQ(progress_of(mode).remaining, Millis{10'000});
        }

        TEST(TimedModeTest, FinishesExactlyAtTheConfiguredDuration) {
            TimedMode mode{Millis{10'000}};
            testing::ModeDriver driver{kText, mode};

            driver.type("t");
            driver.tick(19, Millis{500});
            EXPECT_FALSE(mode.is_finished()) << "9.5 s in";

            driver.tick(1, Millis{500});
            EXPECT_TRUE(mode.is_finished()) << "10 s in, to the millisecond";
        }

        TEST(TimedModeTest, FinishedStaysFinished) {
            TimedMode mode{Millis{1'000}};
            testing::ModeDriver driver{kText, mode};

            driver.type("t");
            driver.tick(2, Millis{1'000});
            ASSERT_TRUE(mode.is_finished());

            driver.tick(10, Millis{1'000});
            driver.type("he");

            EXPECT_TRUE(mode.is_finished());
        }

        TEST(TimedModeTest, ProgressReportsTheCountdownAtEveryPoint) {
            TimedMode mode{Millis{10'000}};
            testing::ModeDriver driver{kText, mode};

            driver.type("t");
            EXPECT_EQ(progress_of(mode).remaining, Millis{10'000}) << "0%";

            driver.tick(5, Millis{1'000});
            EXPECT_EQ(progress_of(mode).elapsed, Millis{5'000}) << "50%";
            EXPECT_EQ(progress_of(mode).remaining, Millis{5'000});

            driver.tick(5, Millis{1'000});
            EXPECT_EQ(progress_of(mode).elapsed, Millis{10'000}) << "100%";
            EXPECT_EQ(progress_of(mode).remaining, Millis{0});
        }

        TEST(TimedModeTest, RemainingTimeNeverGoesNegative) {
            TimedMode mode{Millis{1'000}};
            testing::ModeDriver driver{kText, mode};

            driver.type("t");
            driver.tick(60, Millis{1'000});

            EXPECT_EQ(progress_of(mode).remaining, Millis{0});
            EXPECT_GT(progress_of(mode).elapsed, Millis{1'000}) << "elapsed is honest about how long it really was";
        }

        TEST(TimedModeTest, AKeystrokeAdvancesTheClockAsSurelyAsATick) {
            // A run where nothing ticks — a frozen frame ticker — still ends.
            TimedMode mode{Millis{1'000}};
            testing::ModeDriver driver{kText, mode};

            driver.type("t");
            driver.type("h", Millis{2'000});

            EXPECT_TRUE(mode.is_finished());
        }

        TEST(TimedModeTest, TheValidatedBoundsBothBehave) {
            TimedMode shortest{TimedMode::kMinDuration};
            testing::ModeDriver short_run{kText, shortest};
            short_run.type("t");
            short_run.tick(1, Millis{1'000});
            EXPECT_TRUE(shortest.is_finished()) << "one second";

            TimedMode longest{TimedMode::kMaxDuration};
            testing::ModeDriver long_run{kText, longest};
            long_run.type("t");
            long_run.tick(3'599, Millis{1'000});
            EXPECT_FALSE(longest.is_finished()) << "3599 s of an hour";
            long_run.tick(1, Millis{1'000});
            EXPECT_TRUE(longest.is_finished()) << "3600 s";
        }

        TEST(TimedModeDeathTest, ADurationOutsideTheValidatedRangeIsABug) {
            TYPEIT_EXPECT_PRECONDITION(TimedMode{Millis{0}}, "validated range");
            TYPEIT_EXPECT_PRECONDITION(TimedMode{Millis{3'600'001}}, "validated range");
        }

        // Ported from tests/test_timer.cpp. All seven cases, with the FakeClock
        // discipline replacing every sleep_for: the legacy suite spends eight
        // real seconds racing the scheduler to assert the string "9s".
        TEST(TimedModeTest, LegacyElapsedTimeStartsAtZero) {
            TimedMode mode{Millis{10'000}};
            testing::ModeDriver driver{kText, mode};

            driver.type("t");

            EXPECT_EQ(progress_of(mode).elapsed, Millis{0});
        }

        TEST(TimedModeTest, LegacyElapsedTimeAfterDelay) {
            TimedMode mode{Millis{10'000}};
            testing::ModeDriver driver{kText, mode};

            driver.type("t");
            driver.tick(1, Millis{1'000});

            EXPECT_GE(progress_of(mode).elapsed, Millis{1'000});
        }

        TEST(TimedModeTest, LegacyElapsedTimeAfterMaxTime) {
            TimedMode mode{Millis{1'000}};
            testing::ModeDriver driver{kText, mode};

            driver.type("t");
            driver.tick(2, Millis{1'000});

            EXPECT_GE(progress_of(mode).elapsed, Millis{1'000});
        }

        TEST(TimedModeTest, LegacyRemainingTimeImmediate) {
            TimedMode mode{Millis{10'000}};
            testing::ModeDriver driver{kText, mode};

            driver.type("t");

            EXPECT_EQ(progress_of(mode).remaining, Millis{10'000}) << R"(the legacy "10s")";
        }

        TEST(TimedModeTest, LegacyRemainingTimeAfterOneSecond) {
            TimedMode mode{Millis{10'000}};
            testing::ModeDriver driver{kText, mode};

            driver.type("t");
            driver.tick(1, Millis{1'000});

            EXPECT_EQ(progress_of(mode).remaining, Millis{9'000}) << R"(the legacy "9s")";
        }

        TEST(TimedModeTest, LegacyRemainingTimeAfterMaxTime) {
            TimedMode mode{Millis{1'000}};
            testing::ModeDriver driver{kText, mode};

            driver.type("t");
            driver.tick(2, Millis{1'000});

            EXPECT_EQ(progress_of(mode).remaining, Millis{0}) << R"(the legacy "0s")";
        }

        TEST(TimedModeTest, LegacyDoesNotCountBeforeTheRunStarts) {
            TimedMode mode{Millis{10'000}};
            testing::ModeDriver driver{kText, mode};

            driver.tick(2, Millis{1'000});

            EXPECT_EQ(progress_of(mode).remaining, Millis{10'000});
            EXPECT_FALSE(mode.has_started());
        }

    }  // namespace
}  // namespace typeit::core
