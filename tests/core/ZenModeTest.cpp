#include <gtest/gtest.h>
#include <variant>

#include "typeit/core/metrics/Metrics.h"
#include "typeit/core/modes/IMode.h"
#include "typeit/core/modes/ZenMode.h"
#include "typeit/core/util/Units.h"
#include "typeit/testing/ModeDriver.h"

namespace typeit::core {
    namespace {

        OpenProgress progress_of(const ZenMode& mode) { return std::get<OpenProgress>(mode.progress()); }

        TEST(ZenModeTest, IsRegisteredAndPersistedAsZen) {
            const ZenMode mode;

            EXPECT_EQ(mode.id(), "zen");
        }

        TEST(ZenModeTest, NeverFinishesOnItsOwn) {
            ZenMode mode;
            testing::ModeDriver driver{"one two three", mode};

            driver.type("one two three");
            driver.tick(1'000, Millis{1'000});
            driver.type("and on and on");

            EXPECT_FALSE(mode.is_finished()) << "the text ran out, the run did not";
        }

        TEST(ZenModeTest, AnEmptyTextIsStillNotFinished) {
            ZenMode mode;
            testing::ModeDriver driver{"", mode};

            driver.tick(10, Millis{1'000});

            EXPECT_FALSE(mode.is_finished());
        }

        TEST(ZenModeTest, ProgressReportsElapsedRatherThanRemaining) {
            ZenMode mode;
            testing::ModeDriver driver{"abc", mode};

            EXPECT_EQ(progress_of(mode).elapsed, Millis{0});

            driver.tick(30, Millis{1'000});

            EXPECT_EQ(progress_of(mode).elapsed, Millis{30'000}) << "there is no total to be a fraction of";
        }

        TEST(ZenModeTest, KeystrokesAdvanceTheClockToo) {
            ZenMode mode;
            testing::ModeDriver driver{"abc", mode};

            driver.type("abc", Millis{1'000});

            EXPECT_EQ(progress_of(mode).elapsed, Millis{3'000});
        }

        TEST(ZenModeTest, MetricsAreStillRecorded) {
            // A run with no end still has a log, and everything is derived from
            // the log (ADR-002).
            ZenMode mode;
            testing::ModeDriver driver{"abcde", mode};

            driver.type("abxde", Millis{200});

            const SpeedMetrics speeds = speed(driver.model().log(), driver.target());
            const AccuracyMetrics correctness = accuracy(driver.model().log(), driver.target());

            EXPECT_GT(speeds.raw.value, 0.0);
            EXPECT_EQ(correctness.attempted, 5U);
            EXPECT_EQ(correctness.first_attempt_errors, 1U);
        }

    }  // namespace
}  // namespace typeit::core
