#include <cstddef>
#include <gtest/gtest.h>
#include <random>
#include <string>
#include <string_view>
#include <vector>

#include "typeit/core/metrics/Metrics.h"
#include "typeit/core/session/Keystroke.h"
#include "typeit/core/session/KeystrokeLog.h"
#include "typeit/core/text/TextBuffer.h"
#include "typeit/core/util/Units.h"
#include "typeit/testing/LogBuilder.h"
#include "typeit/testing/TypingRun.h"

namespace typeit::core {
    namespace {

        /// A text of exactly `graphemes` typeable characters, so the worked
        /// examples below are arithmetic rather than counting.
        std::string letters(std::size_t graphemes) { return std::string(graphemes, 'a'); }

        SpeedMetrics speed_of(const KeystrokeLog& log, std::string_view text) {
            return speed(log, testing::text_of(text));
        }

        // The three hand-worked examples. A word is five graphemes, so:
        //
        //   60 graphemes in 60 s  = 12 words in 1 minute  = 12 WPM
        //   300 graphemes in 60 s = 60 words in 1 minute  = 60 WPM
        //   150 graphemes in 30 s = 30 words in 0.5 min   = 60 WPM
        //
        // perfect() places the first keystroke at zero and the last at the full
        // duration, so the elapsed time is exactly the one written here.
        TEST(MetricsSpeedTest, SixtyGraphemesInSixtySecondsIsTwelveWordsPerMinute) {
            const std::string text = letters(60);
            const KeystrokeLog log = testing::perfect(text, Wpm{12.0});

            ASSERT_EQ(log.duration(), Millis{60'000});
            EXPECT_NEAR(speed_of(log, text).gross.value, 12.0, 0.001);
        }

        TEST(MetricsSpeedTest, ThreeHundredGraphemesInSixtySecondsIsSixtyWordsPerMinute) {
            const std::string text = letters(300);
            const KeystrokeLog log = testing::perfect(text, Wpm{60.0});

            ASSERT_EQ(log.duration(), Millis{60'000});
            EXPECT_NEAR(speed_of(log, text).gross.value, 60.0, 0.001);
        }

        TEST(MetricsSpeedTest, HalfTheTimeIsTwiceTheSpeed) {
            const std::string text = letters(150);
            const KeystrokeLog log = testing::perfect(text, Wpm{60.0});

            ASSERT_EQ(log.duration(), Millis{30'000});
            EXPECT_NEAR(speed_of(log, text).gross.value, 60.0, 0.001);
        }

        TEST(MetricsSpeedTest, APerfectRunHasThreeIdenticalSpeeds) {
            const std::string text = letters(300);
            const SpeedMetrics metrics = speed_of(testing::perfect(text, Wpm{60.0}), text);

            EXPECT_NEAR(metrics.raw.value, 60.0, 0.001);
            EXPECT_NEAR(metrics.gross.value, 60.0, 0.001);
            EXPECT_NEAR(metrics.net.value, 60.0, 0.001) << "nothing to penalise";
        }

        TEST(MetricsSpeedTest, RawIncludesErrorsGrossExcludesThemAndNetPenalisesThem) {
            // 300 graphemes in a minute with 30 of them wrong and left wrong:
            //   raw   = 300/5 / 1 = 60
            //   gross = 270/5 / 1 = 54
            //   net   = 54 − 30/1 = 24
            const std::string text = letters(300);
            const SpeedMetrics metrics = speed_of(testing::with_errors(text, 30, Wpm{60.0}), text);

            EXPECT_NEAR(metrics.raw.value, 60.0, 0.001);
            EXPECT_NEAR(metrics.gross.value, 54.0, 0.001);
            EXPECT_NEAR(metrics.net.value, 24.0, 0.001);
        }

        TEST(MetricsSpeedTest, ACorrectedErrorCostsTimeAndNothingElse) {
            // The C4 story told in WPM: the mistake is gone from the text, so
            // gross and net do not know about it. Only raw, which counts every
            // key pressed, remembers.
            const KeystrokeLog log = testing::LogBuilder{}
                                             .type("a", Millis{0})
                                             .type("x", Millis{100})
                                             .backspace(Millis{200})
                                             .type("b", Millis{300})
                                             .build();
            const SpeedMetrics metrics = speed_of(log, "ab");

            // Three graphemes entered, two positions right, none wrong, in 0.3 s
            // — which is 1/200 of a minute.
            EXPECT_NEAR(metrics.raw.value, 3.0 / 5.0 * 200.0, 0.001);
            EXPECT_NEAR(metrics.gross.value, 2.0 / 5.0 * 200.0, 0.001);
            EXPECT_NEAR(metrics.net.value, metrics.gross.value, 0.001);
        }

        TEST(MetricsSpeedTest, ADeletedPositionIsNeitherRightNorWrong) {
            const KeystrokeLog log =
                    testing::LogBuilder{}.type("a", Millis{0}).type("x", Millis{100}).backspace(Millis{200}).build();
            const SpeedMetrics metrics = speed_of(log, "ab");

            EXPECT_NEAR(metrics.gross.value, metrics.net.value, 0.001) << "nothing stands at the deleted position";
        }

        TEST(MetricsSpeedTest, NetIsFlooredAtZero) {
            // Every one of 60 graphemes wrong, in a minute: gross 0, penalty 60.
            const std::string text = letters(60);
            const SpeedMetrics metrics = speed_of(testing::with_errors(text, 60, Wpm{12.0}), text);

            EXPECT_NEAR(metrics.gross.value, 0.0, 0.001);
            EXPECT_EQ(metrics.net.value, 0.0) << "a bad run is not worse than not typing";
        }

        TEST(MetricsSpeedTest, AnEmptyLogIsAllZerosAndNoNaN) {
            const SpeedMetrics metrics = speed_of(KeystrokeLog{}, "abc");

            EXPECT_EQ(metrics.raw.value, 0.0);
            EXPECT_EQ(metrics.gross.value, 0.0);
            EXPECT_EQ(metrics.net.value, 0.0);
        }

        TEST(MetricsSpeedTest, ASingleKeystrokeIsFinite) {
            // It spans no time, and no time is not a denominator.
            const SpeedMetrics metrics = speed_of(testing::LogBuilder{}.type("a", Millis{500}).build(), "abc");

            EXPECT_EQ(metrics.raw.value, 0.0);
            EXPECT_EQ(metrics.gross.value, 0.0);
            EXPECT_EQ(metrics.net.value, 0.0);
        }

        TEST(MetricsSpeedTest, AnEmptyTargetIsNotADivisionByZero) {
            const SpeedMetrics metrics =
                    speed_of(testing::LogBuilder{}.type("a", Millis{0}).type("b", Millis{1'000}).build(), "");

            EXPECT_NEAR(metrics.raw.value, 2.0 / 5.0 * 60.0, 0.001) << "typed into the void, but typed";
            EXPECT_EQ(metrics.gross.value, 0.0);
            EXPECT_EQ(metrics.net.value, 0.0);
        }

        // The two properties, over logs built from random shapes rather than
        // from the cases anyone thought to write down.
        TEST(MetricsSpeedPropertyTest, NetNeverExceedsGrossAndGrossNeverExceedsRaw) {
            std::mt19937 random{20260803};
            std::uniform_int_distribution<std::size_t> pick_length{1, 400};
            std::uniform_real_distribution<double> pick_speed{5.0, 200.0};

            for (int run = 0; run < 500; ++run) {
                const std::size_t length = pick_length(random);
                const std::string text = letters(length);
                std::uniform_int_distribution<std::size_t> pick_errors{0, length};
                const KeystrokeLog log = testing::with_errors(text, pick_errors(random), Wpm{pick_speed(random)});

                const SpeedMetrics metrics = speed_of(log, text);
                ASSERT_LE(metrics.net.value, metrics.gross.value + 1e-9) << "run " << run;
                ASSERT_LE(metrics.gross.value, metrics.raw.value + 1e-9) << "run " << run;
                ASSERT_GE(metrics.net.value, 0.0) << "run " << run;
            }
        }

        TEST(MetricsSpeedPropertyTest, DoublingEveryTimestampHalvesEverySpeed) {
            const std::string text = letters(200);
            const KeystrokeLog original = testing::with_errors(text, 20, Wpm{75.0});

            testing::LogBuilder slower;
            for (const Keystroke& event: original.events()) {
                // Rebuilt rather than mutated: the log has no way to change an
                // event, which is the point of it.
                if (event.kind == KeystrokeKind::Character) {
                    slower.type(event.typed.view(), Millis{event.at.value * 2});
                } else {
                    slower.backspace(Millis{event.at.value * 2});
                }
            }

            const SpeedMetrics fast = speed_of(original, text);
            const SpeedMetrics slow = speed_of(slower.build(), text);

            EXPECT_NEAR(slow.raw.value, fast.raw.value / 2.0, 0.001);
            EXPECT_NEAR(slow.gross.value, fast.gross.value / 2.0, 0.001);
            EXPECT_NEAR(slow.net.value, fast.net.value / 2.0, 0.001);
        }

        // Deliberate divergence from tests/test_word_calculator.cpp.
        //
        // The legacy engine takes a whitespace-delimited count of whatever the
        // user typed — wrong words included — over an elapsed time clamped to
        // the configured duration, and calls that WPM. Its four tests assert
        // that arithmetic directly: 60 "words" in 60 s is 60 WPM.
        //
        // Here the same run is 12 WPM, because 60 graphemes is 12 five-grapheme
        // words, and that is the convention every other typing test uses. The
        // old numbers are not reproduced; they were not comparable with
        // anything.
        TEST(MetricsSpeedTest, DivergesFromTheLegacyWordCalculator) {
            const std::string text = letters(60);
            const KeystrokeLog log = testing::perfect(text, Wpm{12.0});

            ASSERT_EQ(log.duration(), Millis{60'000});
            EXPECT_NEAR(speed_of(log, text).gross.value, 12.0, 0.001) << "the legacy engine would have said 60";

            // And the legacy zero case, which does agree: no time, no speed.
            EXPECT_EQ(speed_of(KeystrokeLog{}, text).gross.value, 0.0);
        }

    }  // namespace
}  // namespace typeit::core
