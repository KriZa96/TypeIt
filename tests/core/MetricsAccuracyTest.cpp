#include <cstddef>
#include <gtest/gtest.h>
#include <random>
#include <string>
#include <string_view>

#include "typeit/core/metrics/Metrics.h"
#include "typeit/core/session/KeystrokeLog.h"
#include "typeit/core/text/TextBuffer.h"
#include "typeit/core/util/Units.h"
#include "typeit/testing/LogBuilder.h"
#include "typeit/testing/TypingRun.h"

namespace typeit::core {
    namespace {

        AccuracyMetrics accuracy_of(const KeystrokeLog& log, std::string_view text) {
            return accuracy(log, testing::text_of(text));
        }

        /// Types `text` correctly, then deletes all of it and types it again,
        /// `passes` times over. The shape defect C4 punishes and this one must
        /// not.
        KeystrokeLog retyped(std::string_view text, std::size_t passes) {
            const TextBuffer target = testing::text_of(text);
            testing::LogBuilder builder;
            Millis now{0};
            const auto tick = [&now] {
                now += Millis{50};
                return now;
            };

            for (std::size_t pass = 0; pass < passes; ++pass) {
                for (std::size_t i = 0; i < target.size(); ++i) {
                    builder.type(target.at(GraphemeIndex{i}).view(), tick());
                }
                if (pass + 1 < passes) {
                    for (std::size_t i = 0; i < target.size(); ++i) {
                        builder.backspace(tick());
                    }
                }
            }
            return builder.build();
        }

        // The three properties that are the definition of done for this issue.

        TEST(MetricsAccuracyTest, APerfectRunIsOneHundredPercentHoweverManyBackspacesItHas) {
            for (const std::size_t passes: {1U, 5U, 50U}) {
                const AccuracyMetrics metrics =
                        accuracy_of(retyped("the quick brown fox", passes), "the quick brown fox");

                EXPECT_EQ(metrics.accuracy.value, 1.0) << passes << " passes";
                EXPECT_EQ(metrics.final_correctness.value, 1.0) << passes << " passes";
                EXPECT_EQ(metrics.first_attempt_errors, 0U) << passes << " passes";
            }
        }

        TEST(MetricsAccuracyTest, TypingAndDeletingThePassageDoesNotChangeTheDenominator) {
            // The other half of C4: holding backspace used to inflate the
            // denominator without bound.
            const std::size_t once = accuracy_of(retyped("the quick brown fox", 1), "the quick brown fox").attempted;

            EXPECT_EQ(accuracy_of(retyped("the quick brown fox", 5), "the quick brown fox").attempted, once);
            EXPECT_EQ(accuracy_of(retyped("the quick brown fox", 50), "the quick brown fox").attempted, once);
            EXPECT_EQ(once, testing::text_of("the quick brown fox").size());
        }

        TEST(MetricsAccuracyTest, RegressionForDefectC4WrongThenBackspaceThenRightIsOneAttempt) {
            // Defect C4 exactly: push_character_accuracy() appended on every
            // keystroke and remove_element() never removed, so this run scored
            // 50% on a position that ends up perfectly typed — permanently,
            // with no way to recover. Deriving from the log makes it one
            // attempt, marked as a first-attempt error.
            const KeystrokeLog log =
                    testing::LogBuilder{}.type("x", Millis{0}).backspace(Millis{100}).type("a", Millis{200}).build();
            const AccuracyMetrics metrics = accuracy_of(log, "a");

            EXPECT_EQ(metrics.attempted, 1U) << "one position, one attempt";
            EXPECT_EQ(metrics.first_attempt_errors, 1U);
            EXPECT_EQ(metrics.accuracy.value, 0.0);
            EXPECT_EQ(metrics.final_correctness.value, 1.0) << "the text is right even though the typing was not";
        }

        TEST(MetricsAccuracyTest, OneErrorInAHundredGraphemesIsNinetyNinePercent) {
            const std::string text(100, 'a');
            const AccuracyMetrics metrics = accuracy_of(testing::with_errors(text, 1), text);

            EXPECT_EQ(metrics.attempted, 100U);
            EXPECT_EQ(metrics.first_attempt_errors, 1U);
            EXPECT_NEAR(metrics.accuracy.value, 0.99, 1e-12);
        }

        TEST(MetricsAccuracyTest, FinalCorrectnessReachesOneWhileAccuracyDoesNot) {
            // Every mistake corrected: the finished text is perfect, the typing
            // was not, and both facts are reported.
            const KeystrokeLog log = testing::LogBuilder{}
                                             .type("a", Millis{0})
                                             .type("x", Millis{100})
                                             .backspace(Millis{200})
                                             .type("b", Millis{300})
                                             .type("c", Millis{400})
                                             .build();
            const AccuracyMetrics metrics = accuracy_of(log, "abc");

            EXPECT_EQ(metrics.final_correctness.value, 1.0);
            EXPECT_NEAR(metrics.accuracy.value, 2.0 / 3.0, 1e-12);
        }

        TEST(MetricsAccuracyTest, AnEmptyLogIsZeroAndNotNaN) {
            const AccuracyMetrics metrics = accuracy_of(KeystrokeLog{}, "abc");

            EXPECT_EQ(metrics.accuracy.value, 0.0);
            EXPECT_EQ(metrics.final_correctness.value, 0.0);
            EXPECT_EQ(metrics.attempted, 0U);
        }

        TEST(MetricsAccuracyTest, AnEmptyTargetIsZeroAndNotNaN) {
            const AccuracyMetrics metrics = accuracy_of(testing::LogBuilder{}.type("a", Millis{0}).build(), "");

            EXPECT_EQ(metrics.accuracy.value, 0.0);
            EXPECT_EQ(metrics.final_correctness.value, 0.0);
        }

        TEST(MetricsAccuracyTest, AllWrongIsZeroOnBothCounts) {
            const std::string text(50, 'a');
            const AccuracyMetrics metrics = accuracy_of(testing::with_errors(text, 50), text);

            EXPECT_EQ(metrics.accuracy.value, 0.0);
            EXPECT_EQ(metrics.final_correctness.value, 0.0);
            EXPECT_EQ(metrics.first_attempt_errors, 50U);
        }

        TEST(MetricsAccuracyTest, ADeletedPositionIsStillAnAttempt) {
            // Typed, thought better of it, left blank: the attempt happened,
            // and nothing stands there to be right or wrong at the end.
            const KeystrokeLog log =
                    testing::LogBuilder{}.type("a", Millis{0}).type("x", Millis{100}).backspace(Millis{200}).build();
            const AccuracyMetrics metrics = accuracy_of(log, "ab");

            EXPECT_EQ(metrics.attempted, 2U);
            EXPECT_EQ(metrics.first_attempt_errors, 1U);
            EXPECT_EQ(metrics.final_correctness.value, 1.0) << "one position resolved, and it is right";
        }

        TEST(MetricsAccuracyPropertyTest, BothValuesStayInTheUnitInterval) {
            std::mt19937 random{20260803};
            std::uniform_int_distribution<std::size_t> pick_length{1, 200};

            for (int run = 0; run < 500; ++run) {
                const std::size_t length = pick_length(random);
                const std::string text(length, 'a');
                std::uniform_int_distribution<std::size_t> pick_errors{0, length};
                const AccuracyMetrics metrics = accuracy_of(testing::with_errors(text, pick_errors(random)), text);

                ASSERT_GE(metrics.accuracy.value, 0.0) << "run " << run;
                ASSERT_LE(metrics.accuracy.value, 1.0) << "run " << run;
                ASSERT_GE(metrics.final_correctness.value, 0.0) << "run " << run;
                ASSERT_LE(metrics.final_correctness.value, 1.0) << "run " << run;
            }
        }

        TEST(MetricsAccuracyPropertyTest, TheSameLogAlwaysGivesTheSameAnswer) {
            const std::string text = "the quick brown fox jumps over the lazy dog";
            const KeystrokeLog log = testing::with_errors(text, 7);

            const AccuracyMetrics first = accuracy_of(log, text);
            const AccuracyMetrics second = accuracy_of(log, text);

            EXPECT_EQ(first.accuracy, second.accuracy);
            EXPECT_EQ(first.final_correctness, second.final_correctness);
            EXPECT_EQ(first.attempted, second.attempted);
        }

        // Deliberate divergence from tests/test_input_accuracy.cpp.
        //
        // Those three tests assert InputAccuracyEngine's behaviour: a vector of
        // booleans pushed once per keystroke, with a remove_element() that never
        // removes. Their expectations encode defect C4 — two keystrokes at one
        // position are two samples, so the corrected run scores 50% forever.
        //
        // The two cases worth keeping do carry over, and are asserted here.
        TEST(MetricsAccuracyTest, KeepsTheLegacyCasesThatWereRight) {
            const std::string text(100, 'a');

            // "100 correct keystrokes is 100%" — still true.
            EXPECT_EQ(accuracy_of(testing::perfect(text, Wpm{60.0}), text).accuracy.value, 1.0);

            // "nothing typed yet is 0%" — still true, and still not a NaN.
            EXPECT_EQ(accuracy_of(KeystrokeLog{}, text).accuracy.value, 0.0);

            // "one right, one wrong is 50%" — true here only because neither was
            // corrected. The legacy engine says 50% even after the mistake is
            // fixed, which is the defect.
            const KeystrokeLog half = testing::LogBuilder{}.type("a", Millis{0}).type("x", Millis{100}).build();
            EXPECT_EQ(accuracy_of(half, "aa").accuracy.value, 0.5);
        }

    }  // namespace
}  // namespace typeit::core
