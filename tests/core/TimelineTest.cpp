#include <cstddef>
#include <cstdint>
#include <gtest/gtest.h>
#include <string>
#include <string_view>
#include <vector>

#include "typeit/core/metrics/Timeline.h"
#include "typeit/core/session/KeystrokeLog.h"
#include "typeit/core/text/TextBuffer.h"
#include "typeit/core/util/Units.h"
#include "typeit/testing/LogBuilder.h"
#include "typeit/testing/TypingRun.h"

namespace typeit::core {
    namespace {

        std::vector<TimelineSample> timeline_of(const KeystrokeLog& log, std::string_view text) {
            return timeline(log, testing::text_of(text));
        }

        KeystrokeLog steady(std::size_t count, std::int64_t every) {
            testing::LogBuilder builder;
            for (std::size_t i = 0; i < count; ++i) {
                builder.type("a", Millis{static_cast<std::int64_t>(i) * every});
            }
            return builder.build();
        }

        TEST(TimelineTest, AThirtySecondRunIsThirtySamples) {
            // 150 graphemes, one every 200 ms: the last lands at 29 800 ms, so
            // the run spans just under thirty seconds and fills thirty buckets.
            const std::string text(150, 'a');

            EXPECT_EQ(timeline_of(steady(150, 200), text).size(), 30U);
        }

        TEST(TimelineTest, TheLastEventOnABoundaryStaysInTheBucketItEnds) {
            // Exactly 30 000 ms: the closing boundary. Thirty samples, not
            // thirty-one with one keystroke in it.
            const std::string text(31, 'a');
            const std::vector<TimelineSample> samples = timeline_of(steady(31, 1'000), text);

            ASSERT_EQ(samples.size(), 30U);
            EXPECT_EQ(samples.back().keystrokes, 2U) << "the 29th second holds its own event and the closing one";
        }

        TEST(TimelineTest, BucketsAreHalfOpen) {
            // Events at 0, 999, 1 000 and 1 001: the first two in the opening
            // bucket, the last two in the next.
            const KeystrokeLog log = testing::LogBuilder{}
                                             .type("a", Millis{0})
                                             .type("a", Millis{999})
                                             .type("a", Millis{1'000})
                                             .type("a", Millis{1'001})
                                             .build();
            const std::vector<TimelineSample> samples = timeline_of(log, std::string(4, 'a'));

            ASSERT_EQ(samples.size(), 2U);
            EXPECT_EQ(samples[0].keystrokes, 2U);
            EXPECT_EQ(samples[1].keystrokes, 2U);
        }

        TEST(TimelineTest, ASampleReportsWhenItOpens) {
            const std::vector<TimelineSample> samples = timeline_of(steady(31, 1'000), std::string(31, 'a'));

            ASSERT_EQ(samples.size(), 30U);
            EXPECT_EQ(samples[0].at, Millis{0});
            EXPECT_EQ(samples[1].at, Millis{1'000});
            EXPECT_EQ(samples[29].at, Millis{29'000});
        }

        TEST(TimelineTest, WpmIsTheGrossRateOfThatSecond) {
            // Five correct graphemes in a second is one word a second, which is
            // 60 WPM.
            const std::string text(150, 'a');
            const std::vector<TimelineSample> samples = timeline_of(steady(150, 200), text);

            for (const TimelineSample& sample: samples) {
                EXPECT_NEAR(sample.wpm.value, 60.0, 1e-9) << "at " << sample.at.value;
            }
        }

        TEST(TimelineTest, ErrorsAreAttributedToTheSecondTheyWereMadeIn) {
            const KeystrokeLog log = testing::LogBuilder{}
                                             .type("a", Millis{0})
                                             .type("x", Millis{100})  // wrong, in the first second
                                             .type("a", Millis{1'500})  // right, in the second
                                             .type("x", Millis{1'700})  // wrong, in the second
                                             .type("x", Millis{1'900})  // wrong, in the second
                                             .type("a", Millis{2'500})
                                             .build();
            const std::vector<TimelineSample> samples = timeline_of(log, std::string(6, 'a'));

            ASSERT_EQ(samples.size(), 3U);
            EXPECT_EQ(samples[0].errors, 1U);
            EXPECT_EQ(samples[1].errors, 2U);
            EXPECT_EQ(samples[2].errors, 0U);
        }

        TEST(TimelineTest, ACorrectedErrorStillBelongsToTheSecondItHappenedIn) {
            // The correction is a later event in a later bucket; it does not
            // reach back and tidy the record of the second the mistake was made.
            const KeystrokeLog log = testing::LogBuilder{}
                                             .type("x", Millis{0})
                                             .backspace(Millis{1'200})
                                             .type("a", Millis{1'400})
                                             .type("b", Millis{2'400})
                                             .build();
            const std::vector<TimelineSample> samples = timeline_of(log, "ab");

            ASSERT_EQ(samples.size(), 3U);
            EXPECT_EQ(samples[0].errors, 1U);
            EXPECT_EQ(samples[1].errors, 0U);
            EXPECT_EQ(samples[1].keystrokes, 2U) << "the backspace and the retype";
        }

        TEST(TimelineTest, ARunShorterThanABucketIsOneSample) {
            const std::vector<TimelineSample> samples = timeline_of(steady(5, 100), std::string(5, 'a'));

            ASSERT_EQ(samples.size(), 1U);
            EXPECT_EQ(samples[0].keystrokes, 5U);
        }

        TEST(TimelineTest, AnEmptyLogHasNoSamplesAtAll) {
            EXPECT_TRUE(timeline_of(KeystrokeLog{}, "abc").empty()) << "no run, no chart";
        }

        TEST(TimelineTest, ASingleKeystrokeIsOneSample) {
            const std::vector<TimelineSample> samples =
                    timeline_of(testing::LogBuilder{}.type("a", Millis{500}).build(), "a");

            ASSERT_EQ(samples.size(), 1U);
            EXPECT_EQ(samples[0].at, Millis{500}) << "the clock starts at the first keystroke";
            EXPECT_EQ(samples[0].keystrokes, 1U);
        }

        TEST(TimelineTest, BucketWidthIsConfigurable) {
            const std::string text(150, 'a');
            const KeystrokeLog log = steady(150, 200);

            EXPECT_EQ(timeline(log, testing::text_of(text), Millis{5'000}).size(), 6U);
            EXPECT_EQ(timeline(log, testing::text_of(text), Millis{500}).size(), 60U);
        }

        TEST(TimelinePropertyTest, TheSamplesAccountForEveryEventInTheLog) {
            const std::string text = "the quick brown fox jumps over the lazy dog again and again";

            for (const KeystrokeLog& log: {testing::perfect(text, Wpm{60.0}), testing::bursty(text, Wpm{40.0}),
                                           testing::with_errors(text, 9, Wpm{80.0}),
                                           testing::with_pauses(text, std::vector<Millis>{Millis{9'000}})}) {
                std::size_t counted = 0;
                for (const TimelineSample& sample: timeline_of(log, text)) {
                    counted += sample.keystrokes;
                }
                EXPECT_EQ(counted, log.size());
            }
        }

    }  // namespace
}  // namespace typeit::core
