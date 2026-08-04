#include <cstddef>
#include <cstdint>
#include <gtest/gtest.h>
#include <string>
#include <string_view>

#include "typeit/core/metrics/Metrics.h"
#include "typeit/core/metrics/RollingWpm.h"
#include "typeit/core/session/KeystrokeLog.h"
#include "typeit/core/text/TextBuffer.h"
#include "typeit/core/util/Units.h"
#include "typeit/testing/LogBuilder.h"
#include "typeit/testing/TypingRun.h"

namespace typeit::core {
    namespace {

        /// `count` correct keystrokes `every` milliseconds apart, starting at
        /// `from`. Hand-built because a windowed metric is sensitive to the
        /// spacing to the millisecond.
        void type_run(testing::LogBuilder& builder, std::size_t count, std::int64_t every, std::int64_t from) {
            for (std::size_t i = 0; i < count; ++i) {
                builder.type("a", Millis{from + (static_cast<std::int64_t>(i) * every)});
            }
        }

        KeystrokeLog steady(std::size_t count, std::int64_t every) {
            testing::LogBuilder builder;
            type_run(builder, count, every, 0);
            return builder.build();
        }

        TEST(MetricsRollingTest, ConstantTypingConvergesOnTheTrueRate) {
            // One grapheme every 200 ms is five a second, which is 60 WPM.
            const std::string text(300, 'a');
            const TextBuffer target = testing::text_of(text);
            const KeystrokeLog log = steady(300, 200);

            RollingWpm rolling{target};
            Wpm latest{0.0};
            for (Millis now{0}; now <= Millis{59'800}; now += Millis{1'000}) {
                latest = rolling.advance(log, now);
            }

            EXPECT_NEAR(latest.value, 60.0, 0.5);
        }

        TEST(MetricsRollingTest, AWindowLargerThanTheRunEqualsCumulativeGrossWpm) {
            const std::string text(300, 'a');
            const TextBuffer target = testing::text_of(text);
            const KeystrokeLog log = testing::perfect(text, Wpm{60.0});

            RollingWpm rolling{target, Millis{600'000}};
            const Wpm rolled = rolling.advance(log, log.events().back().at);

            EXPECT_NEAR(rolled.value, speed(log, target).gross.value, 0.001);
        }

        TEST(MetricsRollingTest, ARateChangeShowsUpWithinOneWindow) {
            // 60 WPM for fifteen seconds, then 120 WPM for fifteen more.
            testing::LogBuilder builder;
            type_run(builder, 75, 200, 0);
            type_run(builder, 150, 100, 15'000);
            const KeystrokeLog log = builder.build();
            const std::string text(225, 'a');
            const TextBuffer target = testing::text_of(text);

            RollingWpm rolling{target, Millis{5'000}};
            Wpm before{0.0};
            Wpm after{0.0};
            for (Millis now{0}; now <= Millis{30'000}; now += Millis{500}) {
                const Wpm reading = rolling.advance(log, now);
                if (now == Millis{14'500}) {
                    before = reading;
                }
                if (now == Millis{24'000}) {
                    after = reading;
                }
            }

            EXPECT_NEAR(before.value, 60.0, 1.0);
            EXPECT_NEAR(after.value, 120.0, 1.0) << "the new rate is visible one window later";
        }

        TEST(MetricsRollingTest, GoingIdleDecaysToZero) {
            const std::string text(75, 'a');
            const TextBuffer target = testing::text_of(text);
            const KeystrokeLog log = steady(75, 200);

            RollingWpm rolling{target, Millis{5'000}};
            for (Millis now{0}; now <= Millis{14'000}; now += Millis{500}) {
                static_cast<void>(rolling.advance(log, now));
            }

            // Six seconds after the last keystroke, nothing is left in a
            // five-second window.
            EXPECT_EQ(rolling.advance(log, Millis{21'000}).value, 0.0);
        }

        TEST(MetricsRollingTest, BackspacesAndOverrunsCountForNothing) {
            // Only correct graphemes are a speed. A backspace is not a
            // grapheme, and a keystroke aimed past the end of the text landed
            // on no position to be correct at.
            const TextBuffer target = testing::text_of("ab");
            const KeystrokeLog log = testing::LogBuilder{}
                                             .type("a", Millis{0})
                                             .type("b", Millis{100})
                                             .backspace(Millis{200})
                                             .type("c", Millis{300})
                                             .type("d", Millis{400})
                                             .build();

            RollingWpm rolling{target};
            const Wpm reading = rolling.advance(log, Millis{400});

            // Two correct graphemes in 0.4 s. A live speed counts keystrokes
            // as they land — it is a rate, not a verdict on the finished text —
            // so the a and the b both count, while the backspace, the wrong
            // retype and the overrun count for nothing.
            EXPECT_NEAR(reading.value, 2.0 / 5.0 / (400.0 / 60'000.0), 0.001);
        }

        TEST(MetricsRollingTest, AnEmptyLogIsZeroAtEveryTick) {
            const TextBuffer target = testing::text_of("abc");
            const KeystrokeLog log;

            RollingWpm rolling{target};
            EXPECT_EQ(rolling.advance(log, Millis{0}).value, 0.0);
            EXPECT_EQ(rolling.advance(log, Millis{60'000}).value, 0.0);
        }

        TEST(MetricsRollingTest, TheLeftIndexOnlyMovesForwardAndEachEventIsSeenTwiceAtMost) {
            // The complexity claim, asserted rather than assumed: 100k events,
            // one tick each, and the total work stays linear in the log rather
            // than quadratic. A rescanning implementation would visit five
            // billion events here and would not finish this test.
            constexpr std::size_t kEvents = 100'000;
            const std::string text(kEvents, 'a');
            const TextBuffer target = testing::text_of(text);
            const KeystrokeLog log = steady(kEvents, 50);

            RollingWpm rolling{target};
            std::size_t previous_left = 0;
            for (const Keystroke& event: log.events()) {
                static_cast<void>(rolling.advance(log, event.at));
                ASSERT_GE(rolling.left(), previous_left);
                previous_left = rolling.left();
            }

            EXPECT_LE(rolling.events_visited(), 2 * kEvents) << "each event enters the window once and leaves once";
            EXPECT_GT(rolling.left(), 0U) << "and the window did move";
        }

        TEST(MetricsRollingTest, TickingMoreOftenDoesNotCostMoreWork) {
            const std::string text(1'000, 'a');
            const TextBuffer target = testing::text_of(text);
            const KeystrokeLog log = steady(1'000, 50);

            RollingWpm rare{target};
            for (Millis now{0}; now <= Millis{50'000}; now += Millis{1'000}) {
                static_cast<void>(rare.advance(log, now));
            }

            RollingWpm often{target};
            for (Millis now{0}; now <= Millis{50'000}; now += Millis{10}) {
                static_cast<void>(often.advance(log, now));
            }

            EXPECT_EQ(often.events_visited(), rare.events_visited()) << "work follows the log, not the tick rate";
        }

        TEST(MetricsPeakSustainedTest, ABriefBurstIsNotASustainedSpeed) {
            // Two seconds of very fast typing, then a long slow crawl. The
            // burst never held for ten seconds, so it is not the answer.
            testing::LogBuilder builder;
            type_run(builder, 60, 33, 0);  // ~2 s at roughly 360 WPM
            type_run(builder, 60, 1'000, 3'000);  // then 12 WPM for a minute
            const KeystrokeLog log = builder.build();
            const std::string text(120, 'a');
            const TextBuffer target = testing::text_of(text);

            EXPECT_LT(peak_sustained_wpm(log, target).value, 100.0);
        }

        TEST(MetricsPeakSustainedTest, ATwelveSecondPlateauIsCaptured) {
            // Twelve seconds at 120 WPM, then nothing for a minute.
            testing::LogBuilder builder;
            type_run(builder, 120, 100, 0);
            type_run(builder, 5, 1'000, 60'000);
            const KeystrokeLog log = builder.build();
            const std::string text(125, 'a');
            const TextBuffer target = testing::text_of(text);

            EXPECT_NEAR(peak_sustained_wpm(log, target).value, 120.0, 5.0);
        }

        TEST(MetricsPeakSustainedTest, ARunShorterThanTheThresholdSustainedNothing) {
            const std::string text(20, 'a');
            const TextBuffer target = testing::text_of(text);

            EXPECT_EQ(peak_sustained_wpm(steady(20, 100), target).value, 0.0);
        }

        TEST(MetricsPeakSustainedTest, AnEmptyLogIsZero) {
            const TextBuffer target = testing::text_of("abc");

            EXPECT_EQ(peak_sustained_wpm(KeystrokeLog{}, target).value, 0.0);
        }

    }  // namespace
}  // namespace typeit::core
