// The trend line, and every degenerate series that reaches it (TI-099).
//
// The interesting assertions are all about inputs a real history produces on
// somebody's first week: no runs, one run, a week of identical scores, and more
// runs than there are columns. Each of those is a division by zero or an empty
// range in the obvious implementation.

#include <cstddef>
#include <gtest/gtest.h>
#include <string>
#include <string_view>
#include <vector>

#include "Charts.h"
#include "Glyphs.h"
#include "Snapshot.h"
#include "typeit/app/Capabilities.h"
#include "typeit/app/Theme.h"

namespace typeit::tui {
    namespace {

        /// How many ramp cells a rendered sparkline has. Counted in graphemes
        /// rather than bytes: the Unicode ramp is three bytes a cell, and a
        /// test that counted bytes would pass for the wrong reason.
        std::size_t cells_in(const std::string& drawn) {
            std::size_t count = 0;
            for (const char byte: drawn) {
                // A UTF-8 continuation byte is `10xxxxxx`; everything else
                // starts a character.
                if ((static_cast<unsigned char>(byte) & 0xC0U) != 0x80U) {
                    ++count;
                }
            }
            return count;
        }

        TEST(SparklineTest, EveryValueGetsOneCell) {
            const std::vector<double> values{1.0, 2.0, 3.0, 4.0, 5.0};

            EXPECT_EQ(cells_in(sparkline_text(values, {.width = 40})), values.size());
        }

        TEST(SparklineTest, AllEqualValuesDrawAFlatLineAtTheMiddleRatherThanTheFloor) {
            // A week of identical scores is a flat line. Drawing it at the
            // lowest level says the typist bottomed out, which is false.
            const std::string drawn = sparkline_text({70.0, 70.0, 70.0, 70.0}, {.width = 40});

            ASSERT_EQ(cells_in(drawn), 4U);
            const std::string_view middle = sparkline_level(app::GlyphSet::Unicode, kSparklineLevels / 2);
            const std::string_view lowest = sparkline_level(app::GlyphSet::Unicode, 0);
            EXPECT_EQ(drawn, std::string{middle} + std::string{middle} + std::string{middle} + std::string{middle});
            EXPECT_NE(drawn.find(middle), std::string::npos);
            EXPECT_EQ(drawn.find(lowest), std::string::npos);
        }

        TEST(SparklineTest, ASingleValueDrawsOneCell) {
            EXPECT_EQ(cells_in(sparkline_text({42.0}, {.width = 40})), 1U);
        }

        TEST(SparklineTest, NoValuesDrawNothingAtAll) {
            // Not a row of the lowest level: "no runs" and "a run of zero" are
            // different facts, and the picture must not confuse them.
            EXPECT_TRUE(sparkline_text({}, {.width = 40}).empty());
        }

        TEST(SparklineTest, NegativeValuesAreClampedRatherThanRejected) {
            // Nothing plotted here can legitimately be negative, so a negative
            // is bad data — and losing the whole series over one bad point is
            // worse than flooring it.
            const std::string drawn = sparkline_text({-5.0, 0.0, 10.0}, {.width = 40});

            ASSERT_EQ(cells_in(drawn), 3U);
            const std::string_view lowest = sparkline_level(app::GlyphSet::Unicode, 0);
            EXPECT_TRUE(drawn.starts_with(lowest)) << "the negative and the zero share the floor";
        }

        TEST(SparklineTest, MoreValuesThanCellsDownsampleToTheWidthDeterministically) {
            std::vector<double> many;
            many.reserve(500);
            for (std::size_t at = 0; at < 500; ++at) {
                many.push_back(static_cast<double>(at % 17));
            }

            const std::string first = sparkline_text(many, {.width = 20});
            const std::string second = sparkline_text(many, {.width = 20});

            EXPECT_EQ(cells_in(first), 20U);
            EXPECT_EQ(first, second) << "the same input must give the same picture";
        }

        TEST(SparklineTest, DownsamplingKeepsTheShapeRatherThanSamplingEveryNth) {
            // A rising series must still read as rising after being averaged
            // into buckets — a stride that happened to land on the troughs
            // would draw a flat line over a clear trend.
            std::vector<double> rising;
            rising.reserve(100);
            for (std::size_t at = 0; at < 100; ++at) {
                rising.push_back(static_cast<double>(at));
            }

            const std::string drawn = sparkline_text(rising, {.width = 10});

            EXPECT_EQ(drawn.substr(0, 3), sparkline_level(app::GlyphSet::Unicode, 0));
            EXPECT_TRUE(drawn.ends_with(sparkline_level(app::GlyphSet::Unicode, kSparklineLevels - 1)));
        }

        TEST(SparklineTest, TheAsciiFallbackEmitsNoByteAbove0x7F) {
            const std::string drawn =
                    sparkline_text({1.0, 5.0, 3.0, 9.0, 2.0}, {.width = 40, .glyphs = app::GlyphSet::Ascii});

            ASSERT_FALSE(drawn.empty());
            for (const char byte: drawn) {
                EXPECT_LT(static_cast<unsigned char>(byte), 0x80U) << drawn;
            }
        }

        TEST(SparklineTest, SnapshotOfAKnownSeries) {
            app::Theme theme;
            theme.name = "snapshot";
            const std::vector<double> runs{58.0, 61.0, 60.0, 66.0, 64.0, 71.0, 69.0, 74.0, 72.0, 81.0};

            testing::expect_matches_golden("sparkline_ten_runs",
                                           testing::render_to_text(sparkline(runs, theme, {.width = 10}), 20, 1));
        }

        TEST(ChartPurityTest, NoChartChangesAnythingByDrawing) {
            // Structurally true today — these are free functions over const
            // inputs — and the guard is here for the day one of them grows a
            // cache the way `TypingArea` did. That cache is where 1.0's
            // render-side-effect bug would come back.
            app::Theme theme;
            const std::vector<double> runs{58.0, 61.0, 60.0, 66.0, 64.0};
            LineChartData chart;
            for (std::size_t at = 0; at < runs.size(); ++at) {
                chart.series.push_back({.x = static_cast<double>(at), .y = runs.at(at)});
            }

            testing::expect_render_is_pure([&] { return sparkline(runs, theme, {.width = 20}); }, 20, 1);
            testing::expect_render_is_pure([&] { return histogram(runs, theme, {.width = 20, .height = 4}); }, 20, 4);
            testing::expect_render_is_pure([&] { return line_chart(chart, theme, {.width = 40, .height = 6}); }, 40, 8);
        }

    }  // namespace
}  // namespace typeit::tui
