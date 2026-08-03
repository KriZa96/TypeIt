#include <cstddef>
#include <cstdint>
#include <gtest/gtest.h>
#include <random>
#include <string>
#include <string_view>
#include <vector>

#include "typeit/core/metrics/Metrics.h"
#include "typeit/core/session/KeystrokeLog.h"
#include "typeit/core/text/TextBuffer.h"
#include "typeit/core/util/Units.h"
#include "typeit/testing/LogBuilder.h"
#include "typeit/testing/TypingRun.h"

namespace typeit::core {
    namespace {

        double consistency_of(const KeystrokeLog& log, std::string_view text) {
            return consistency(log, testing::text_of(text));
        }

        /// `count` correct keystrokes, exactly `every` milliseconds apart.
        /// Built by hand rather than with `perfect()` because a bucketed metric
        /// is sensitive to the spacing to the millisecond, and this test is
        /// about what happens when the spacing is exactly even.
        KeystrokeLog evenly_spaced(std::size_t count, std::int64_t every) {
            testing::LogBuilder builder;
            for (std::size_t i = 0; i < count; ++i) {
                builder.type("a", Millis{static_cast<std::int64_t>(i) * every});
            }
            return builder.build();
        }

        TEST(MetricsConsistencyTest, APerfectlyEvenRunIsOneHundred) {
            // 300 graphemes, one every 200 ms: five in every one-second window,
            // for sixty windows. Nothing varies, so σ is zero.
            const std::string text(300, 'a');

            EXPECT_NEAR(consistency_of(evenly_spaced(300, 200), text), 100.0, 1e-9);
        }

        TEST(MetricsConsistencyTest, ABurstyRunWithIdenticalTotalsScoresLower) {
            // The test that proves the metric measures what it claims. Same
            // keystrokes, same elapsed time, same gross WPM — only the spacing
            // differs.
            const std::string text(300, 'a');
            const KeystrokeLog even = testing::perfect(text, Wpm{60.0});
            const KeystrokeLog uneven = testing::bursty(text, Wpm{60.0});

            ASSERT_EQ(uneven.size(), even.size());
            ASSERT_EQ(uneven.duration(), even.duration());

            EXPECT_LT(consistency_of(uneven, text), consistency_of(even, text));
        }

        TEST(MetricsConsistencyTest, APauseCostsMoreThanTheSameKeystrokesWithoutIt) {
            const std::string text(300, 'a');
            const std::vector<Millis> ten_seconds{Millis{10'000}};

            const double unbroken = consistency_of(testing::perfect(text, Wpm{60.0}), text);
            const double interrupted = consistency_of(testing::with_pauses(text, ten_seconds, Wpm{60.0}), text);

            EXPECT_LT(interrupted, unbroken) << "ten idle seconds are ten samples of zero";
        }

        TEST(MetricsConsistencyTest, MatchesTheDefinitionOnAHandComputedExample) {
            // Three one-second windows holding 5, 10 and 0 correct graphemes,
            // which are 60, 120 and 0 WPM.
            //
            //   μ = (60 + 120 + 0) / 3            = 60
            //   σ = sqrt((0² + 60² + 60²) / 3)    = sqrt(2400) = 48.9898…
            //   100 × (1 − 48.9898/60)            = 18.3503…
            testing::LogBuilder builder;
            std::int64_t now = 0;
            for (std::size_t i = 0; i < 5; ++i) {
                builder.type("a", Millis{now});
                now += 100;
            }
            now = 1'000;
            for (std::size_t i = 0; i < 10; ++i) {
                builder.type("a", Millis{now});
                now += 50;
            }
            // One more keystroke at 2 999 ms to open the third window and leave
            // it empty. It is wrong, so it contributes nothing to any sample.
            builder.type("z", Millis{2'999});

            const std::string text(16, 'a');
            EXPECT_NEAR(consistency_of(builder.build(), text), 18.3503, 0.001);
        }

        TEST(MetricsConsistencyTest, ASingleBucketRunIsDefined) {
            // Shorter than one window: there is one sample, so there is nothing
            // to vary and no division by zero on the way to saying so.
            const std::string text(10, 'a');

            EXPECT_NEAR(consistency_of(evenly_spaced(10, 50), text), 100.0, 1e-9);
        }

        TEST(MetricsConsistencyTest, AnEmptyLogIsZero) { EXPECT_EQ(consistency_of(KeystrokeLog{}, "abc"), 0.0); }

        TEST(MetricsConsistencyTest, AnAllWrongRunIsZeroRatherThanUndefined) {
            const std::string text(60, 'a');

            EXPECT_EQ(consistency_of(testing::with_errors(text, 60, Wpm{60.0}), text), 0.0);
        }

        TEST(MetricsConsistencyPropertyTest, TheResultStaysOnItsScale) {
            std::mt19937 random{20260803};
            std::uniform_int_distribution<std::size_t> pick_length{1, 400};
            std::uniform_real_distribution<double> pick_speed{5.0, 200.0};
            std::uniform_int_distribution<int> pick_shape{0, 2};

            for (int run = 0; run < 500; ++run) {
                const std::size_t length = pick_length(random);
                const std::string text(length, 'a');
                const Wpm speed{pick_speed(random)};
                std::uniform_int_distribution<std::size_t> pick_errors{0, length};

                const KeystrokeLog log = [&] {
                    switch (pick_shape(random)) {
                        case 0:
                            return testing::perfect(text, speed);
                        case 1:
                            return testing::bursty(text, speed);
                        default:
                            return testing::with_errors(text, pick_errors(random), speed);
                    }
                }();

                const double result = consistency_of(log, text);
                ASSERT_GE(result, 0.0) << "run " << run;
                ASSERT_LE(result, 100.0) << "run " << run;
            }
        }

    }  // namespace
}  // namespace typeit::core
