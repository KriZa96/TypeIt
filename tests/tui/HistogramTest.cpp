// The WPM distribution (TI-101).
//
// Bucketing is asserted on `bucket_counts` rather than on the picture: "which
// bucket does a boundary value land in" is a question about arithmetic, and
// reading it back out of a column of block characters proves less and breaks
// when the drawing changes.

#include <cstddef>
#include <gtest/gtest.h>
#include <numeric>
#include <string>
#include <vector>

#include "Charts.h"
#include "Snapshot.h"
#include "typeit/app/Capabilities.h"
#include "typeit/app/Theme.h"

namespace typeit::tui {
    namespace {

        TEST(HistogramTest, KnownInputsBucketWhereTheyShould) {
            // 0..10 over five buckets: each bucket is two wide.
            const std::vector<std::size_t> counts = bucket_counts({0.0, 1.0, 2.0, 3.0, 4.0, 10.0}, 5);

            ASSERT_EQ(counts.size(), 5U);
            EXPECT_EQ(counts.at(0), 2U) << "0 and 1";
            EXPECT_EQ(counts.at(1), 2U) << "2 and 3";
            EXPECT_EQ(counts.at(2), 1U) << "4";
            EXPECT_EQ(counts.at(3), 0U);
            EXPECT_EQ(counts.at(4), 1U) << "10, in the closed top bucket";
        }

        TEST(HistogramTest, ABoundaryValueLandsInExactlyOneBucket) {
            // Buckets are half-open upwards, so a value on a boundary belongs
            // to the bucket above it — and every value is counted once.
            const std::vector<double> values{0.0, 2.0, 4.0, 6.0, 8.0, 10.0};

            const std::vector<std::size_t> counts = bucket_counts(values, 5);

            EXPECT_EQ(std::accumulate(counts.begin(), counts.end(), std::size_t{0}), values.size());
            EXPECT_EQ(counts.at(1), 1U) << "2.0 is the floor of the second bucket, not the ceiling of the first";
        }

        TEST(HistogramTest, EveryValueIdenticalIsOneBucketRatherThanADivisionByZero) {
            const std::vector<std::size_t> counts = bucket_counts({70.0, 70.0, 70.0}, 4);

            ASSERT_EQ(counts.size(), 4U);
            EXPECT_EQ(counts.front(), 3U);
            EXPECT_EQ(std::accumulate(counts.begin(), counts.end(), std::size_t{0}), 3U);
        }

        TEST(HistogramTest, NoValuesAndNoBucketsAreBothEmptyRatherThanACrash) {
            EXPECT_TRUE(bucket_counts({}, 5).empty());
            EXPECT_TRUE(bucket_counts({1.0, 2.0}, 0).empty());
        }

        TEST(HistogramTest, TheBucketCountAdaptsToTheWidth) {
            app::Theme theme;
            std::vector<double> values;
            values.reserve(100);
            for (std::size_t at = 0; at < 100; ++at) {
                values.push_back(static_cast<double>(at));
            }

            const std::string narrow = testing::render_to_text(histogram(values, theme, {.width = 20, .height = 4, .glyphs = app::GlyphSet::Ascii}),
                                                               40, 4);
            const std::string wide = testing::render_to_text(histogram(values, theme, {.width = 60, .height = 4, .glyphs = app::GlyphSet::Ascii}),
                                                             80, 4);

            // More columns, more buckets: the wide one draws further across.
            EXPECT_LT(narrow.find_last_not_of(" \n"), wide.find_last_not_of(" \n")) << narrow << "\n---\n" << wide;
        }

        TEST(HistogramTest, ASingleDataPointDrawsOneBar) {
            app::Theme theme;

            const std::string drawn = testing::render_to_text(histogram({70.0}, theme, {.width = 20, .height = 3, .glyphs = app::GlyphSet::Ascii}),
                                                              20, 3);

            // One bucket, full height: the tallest bucket always reaches the
            // top, whatever its absolute count.
            EXPECT_NE(drawn.find('#'), std::string::npos) << drawn;
        }

        TEST(HistogramTest, EmptyInputDrawsNothing) {
            app::Theme theme;

            const std::string drawn = testing::render_to_text(histogram({}, theme, {.width = 20, .height = 3, .glyphs = app::GlyphSet::Ascii}), 20, 3);

            EXPECT_EQ(drawn.find('#'), std::string::npos) << drawn;
        }

        TEST(HistogramTest, BarHeightsScaleToTheTallestBucket) {
            app::Theme theme;
            // Nine values in the bottom bucket, one at the top: the tall bar
            // reaches the ceiling and the short one does not.
            const std::vector<double> lopsided{0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 10.0};

            const std::string drawn =
                    testing::render_to_text(histogram(lopsided, theme, {.width = 10, .height = 5, .glyphs = app::GlyphSet::Ascii}), 20, 5);

            const std::size_t top = drawn.find('#');
            ASSERT_NE(top, std::string::npos) << drawn;
            EXPECT_LT(top, drawn.find('\n')) << "the tallest bucket reaches the first row:\n" << drawn;
        }

    }  // namespace
}  // namespace typeit::tui
