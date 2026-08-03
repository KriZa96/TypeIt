#include <cstddef>
#include <gtest/gtest.h>
#include <string>
#include <string_view>

#include "typeit/core/metrics/KeyStats.h"
#include "typeit/core/session/KeystrokeLog.h"
#include "typeit/core/text/TextBuffer.h"
#include "typeit/core/util/Units.h"
#include "typeit/testing/LogBuilder.h"
#include "typeit/testing/TypingRun.h"

namespace typeit::core {
    namespace {

        KeyStats stats_of(const KeystrokeLog& log, std::string_view text) {
            return key_stats(log, testing::text_of(text));
        }

        TEST(KeyStatsTest, AttemptsAndErrorsTallyPerKey) {
            // Target "aba": a right, b wrong, a right.
            const KeystrokeLog log =
                    testing::LogBuilder{}.type("a", Millis{0}).type("x", Millis{100}).type("a", Millis{200}).build();
            const KeyStats stats = stats_of(log, "aba");

            EXPECT_EQ(stats.per_grapheme.at("a").attempts, 2U);
            EXPECT_EQ(stats.per_grapheme.at("a").errors, 0U);
            EXPECT_EQ(stats.per_grapheme.at("b").attempts, 1U);
            EXPECT_EQ(stats.per_grapheme.at("b").errors, 1U);
        }

        TEST(KeyStatsTest, LatencyIsTheIntervalFromThePreviousKeystroke) {
            const KeystrokeLog log = testing::LogBuilder{}
                                             .type("a", Millis{1'000})
                                             .type("b", Millis{1'150})
                                             .type("c", Millis{1'400})
                                             .build();
            const KeyStats stats = stats_of(log, "abc");

            EXPECT_EQ(stats.per_grapheme.at("a").latency_samples, 0U) << "the first keystroke has no previous one";
            EXPECT_EQ(stats.per_grapheme.at("b").total_latency, Millis{150});
            EXPECT_EQ(stats.per_grapheme.at("c").total_latency, Millis{250});
        }

        TEST(KeyStatsTest, BigramsAreFormedFromConsecutiveTargetPositions) {
            const KeystrokeLog log =
                    testing::LogBuilder{}.type("t", Millis{0}).type("h", Millis{120}).type("e", Millis{200}).build();
            const KeyStats stats = stats_of(log, "the");

            ASSERT_EQ(stats.per_bigram.size(), 2U);
            EXPECT_EQ(stats.per_bigram.at("th").total_latency, Millis{120});
            EXPECT_EQ(stats.per_bigram.at("he").total_latency, Millis{80});
        }

        TEST(KeyStatsTest, ABackspaceDoesNotFabricateABigram) {
            // Typing "ax", deleting the x and typing "b" is not a transition
            // from a to b at speed — there is a whole correction in between,
            // and the position was visited twice.
            const KeystrokeLog log = testing::LogBuilder{}
                                             .type("a", Millis{0})
                                             .type("x", Millis{100})
                                             .backspace(Millis{600})
                                             .type("b", Millis{700})
                                             .build();
            const KeyStats stats = stats_of(log, "ab");

            EXPECT_EQ(stats.per_bigram.count("ab"), 1U) << "the first attempt at the pair does count";
            EXPECT_EQ(stats.per_bigram.at("ab").attempts, 1U) << "and only once, for a→x";
            EXPECT_EQ(stats.per_bigram.at("ab").errors, 1U);
            EXPECT_EQ(stats.per_grapheme.at("b").attempts, 2U) << "the key was tried twice";
        }

        TEST(KeyStatsTest, AbigramIsKeyedByWhatTheTextAskedForNotWhatWasTyped) {
            const KeystrokeLog log = testing::LogBuilder{}.type("t", Millis{0}).type("j", Millis{100}).build();
            const KeyStats stats = stats_of(log, "th");

            EXPECT_EQ(stats.per_bigram.count("tj"), 0U);
            ASSERT_EQ(stats.per_bigram.count("th"), 1U);
            EXPECT_EQ(stats.per_bigram.at("th").errors, 1U);
        }

        TEST(KeyStatsTest, APauseLongerThanTheThresholdIsNotTypingSpeed) {
            // Three seconds is the documented threshold. The keystroke still
            // counts as an attempt; only the interval is thrown away.
            const KeystrokeLog log = testing::LogBuilder{}
                                             .type("a", Millis{0})
                                             .type("b", Millis{5'000})
                                             .type("c", Millis{5'100})
                                             .build();
            const KeyStats stats = stats_of(log, "abc");

            EXPECT_EQ(stats.per_grapheme.at("b").attempts, 1U);
            EXPECT_EQ(stats.per_grapheme.at("b").latency_samples, 0U) << "five seconds is not a keystroke interval";
            EXPECT_EQ(stats.per_grapheme.at("c").latency_samples, 1U);
            EXPECT_EQ(stats.per_bigram.at("ab").latency_samples, 0U);
            EXPECT_EQ(stats.per_bigram.at("bc").total_latency, Millis{100});
        }

        TEST(KeyStatsTest, TheThresholdIsConfigurable) {
            const KeystrokeLog log = testing::LogBuilder{}.type("a", Millis{0}).type("b", Millis{4'000}).build();
            const TextBuffer target = testing::text_of("ab");

            EXPECT_EQ(key_stats(log, target, Millis{3'000}).per_grapheme.at("b").latency_samples, 0U);
            EXPECT_EQ(key_stats(log, target, Millis{10'000}).per_grapheme.at("b").latency_samples, 1U);
        }

        TEST(KeyStatsTest, MeanLatencyIsTheAverageOfTheSamplesKept) {
            const KeystrokeLog log =
                    testing::LogBuilder{}.type("a", Millis{0}).type("a", Millis{100}).type("a", Millis{300}).build();
            const KeyStats stats = stats_of(log, "aaa");

            EXPECT_EQ(stats.per_grapheme.at("a").latency_samples, 2U);
            EXPECT_EQ(stats.per_grapheme.at("a").mean_latency(), Millis{150});
        }

        TEST(KeyStatsTest, AMeanWithNoSamplesIsZeroRatherThanADivisionByZero) {
            const KeyStats stats = stats_of(testing::LogBuilder{}.type("a", Millis{0}).build(), "a");

            EXPECT_EQ(stats.per_grapheme.at("a").mean_latency(), Millis{0});
        }

        TEST(KeyStatsTest, MultiByteGraphemesKeyAsOneKeyEach) {
            const KeystrokeLog log =
                    testing::LogBuilder{}.type("č", Millis{0}).type("š", Millis{100}).type("😀", Millis{200}).build();
            const KeyStats stats = stats_of(log, "čš😀");

            EXPECT_EQ(stats.per_grapheme.size(), 3U) << "not six halves of two-byte characters";
            EXPECT_EQ(stats.per_grapheme.at("č").attempts, 1U);
            EXPECT_EQ(stats.per_bigram.at("čš").total_latency, Millis{100});
            EXPECT_EQ(stats.per_bigram.at("š😀").total_latency, Millis{100});
        }

        TEST(KeyStatsTest, AnEmptyLogIsEmptyStats) {
            const KeyStats stats = stats_of(KeystrokeLog{}, "abc");

            EXPECT_TRUE(stats.per_grapheme.empty());
            EXPECT_TRUE(stats.per_bigram.empty());
        }

        TEST(KeyStatsTest, TypingPastTheEndIsNotAKey) {
            // The overrun space a mode may log against the end of the text
            // belongs to no target position and to no key.
            const KeystrokeLog log =
                    testing::LogBuilder{}.type("a", Millis{0}).type("b", Millis{100}).type("c", Millis{200}).build();
            const KeyStats stats = stats_of(log, "ab");

            EXPECT_EQ(stats.per_grapheme.size(), 2U);
            EXPECT_EQ(stats.per_bigram.size(), 1U);
        }

        TEST(KeyStatsTest, AWholeRunTalliesEveryKeyItTyped) {
            constexpr std::string_view text = "the quick brown fox";
            const KeyStats stats = stats_of(testing::perfect(text, Wpm{60.0}), text);

            std::size_t attempts = 0;
            for (const auto& [grapheme, stat]: stats.per_grapheme) {
                attempts += stat.attempts;
                EXPECT_EQ(stat.errors, 0U) << grapheme;
            }
            EXPECT_EQ(attempts, testing::text_of(text).size());
        }

    }  // namespace
}  // namespace typeit::core
