// The per-second WPM chart (TI-100).
//
// Tick selection is asserted on `axis_ticks` directly: "are these numbers a
// person can read" is a question about a vector, and asking it of a picture
// means parsing the picture.

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <gtest/gtest.h>
#include <string>
#include <vector>

#include "Charts.h"
#include "Snapshot.h"
#include "typeit/app/Theme.h"

namespace typeit::tui {
    namespace {

        /// A rising run: sixty seconds climbing from 40 to 80 WPM.
        std::vector<Point> a_run() {
            std::vector<Point> points;
            points.reserve(60);
            for (std::size_t second = 0; second < 60; ++second) {
                points.push_back(
                        {.x = static_cast<double>(second), .y = 40.0 + static_cast<double>(second) * 2.0 / 3.0});
            }
            return points;
        }

        /// Every gap between consecutive ticks, so "evenly spaced" is one
        /// assertion rather than a loop in each test.
        std::vector<double> gaps(const std::vector<double>& ticks) {
            std::vector<double> out;
            for (std::size_t at = 1; at < ticks.size(); ++at) {
                out.push_back(ticks.at(at) - ticks.at(at - 1));
            }
            return out;
        }

        TEST(LineChartTest, TicksAreReadableNumbersAtEveryScale) {
            // 1, 2, 5 and their powers of ten. A step of 3.7 is evenly spaced
            // and unreadable, which is the whole point of choosing rather than
            // dividing.
            for (const double high: {10.0, 100.0, 1000.0}) {
                const std::vector<double> ticks = axis_ticks(0.0, high, 5);

                ASSERT_GE(ticks.size(), 2U) << "high = " << high;
                for (const double gap: gaps(ticks)) {
                    EXPECT_NEAR(gap, gaps(ticks).front(), 1e-9) << "evenly spaced, high = " << high;
                }
                const double step = gaps(ticks).front();
                const double normalised = step / std::pow(10.0, std::floor(std::log10(step)));
                EXPECT_TRUE(normalised == 1.0 || normalised == 2.0 || normalised == 5.0)
                        << "step " << step << " for range 0.." << high;
            }
        }

        TEST(LineChartTest, AFlatSeriesStillGetsAnAxis) {
            // Not an empty gutter, which reads as a rendering bug rather than
            // as a run that held its speed.
            const std::vector<double> ticks = axis_ticks(70.0, 70.0, 5);

            ASSERT_EQ(ticks.size(), 1U);
            EXPECT_DOUBLE_EQ(ticks.front(), 70.0);
        }

        TEST(LineChartTest, ZeroWantedTicksIsEmptyRatherThanADivisionByZero) {
            EXPECT_TRUE(axis_ticks(0.0, 100.0, 0).empty());
        }

        TEST(LineChartTest, AFlatSeriesRendersWithoutCollapsing) {
            app::Theme theme;
            LineChartData data;
            for (std::size_t second = 0; second < 10; ++second) {
                data.series.push_back({.x = static_cast<double>(second), .y = 70.0});
            }

            const std::string drawn =
                    testing::render_to_text(line_chart(data, theme, {.width = 60, .height = 6}), 60, 8);

            EXPECT_NE(drawn.find('*'), std::string::npos) << drawn;
            EXPECT_NE(drawn.find("70"), std::string::npos) << "the value it held is labelled:\n" << drawn;
        }

        TEST(LineChartTest, AnErrorMarkerLandsAtTheColumnItsTimestampAsksFor) {
            app::Theme theme;
            LineChartData data;
            data.series = a_run();
            // Half way through the run, so the marker must land near the middle
            // of the plot rather than at either end.
            data.errors = {30.0};

            const std::string drawn =
                    testing::render_to_text(line_chart(data, theme, {.width = 60, .height = 6}), 60, 8);

            const std::size_t marker = drawn.find('x');
            ASSERT_NE(marker, std::string::npos) << drawn;
            const std::size_t column = marker - drawn.rfind('\n', marker) - 1;
            EXPECT_GE(column, 25U) << drawn;
            EXPECT_LE(column, 40U) << drawn;
        }

        TEST(LineChartTest, AnOverlaySeriesIsDrawnDistinctlyFromThePrimary) {
            app::Theme theme;
            LineChartData data;
            data.series = a_run();
            for (std::size_t second = 0; second < 60; ++second) {
                data.overlay.push_back({.x = static_cast<double>(second), .y = 75.0});
            }

            const std::string drawn =
                    testing::render_to_text(line_chart(data, theme, {.width = 60, .height = 8}), 60, 12);

            EXPECT_NE(drawn.find('*'), std::string::npos) << "the run:\n" << drawn;
            EXPECT_NE(drawn.find('-'), std::string::npos) << "the pacer, in another mark:\n" << drawn;
            EXPECT_NE(drawn.find("pacer"), std::string::npos) << "and it is named:\n" << drawn;
        }

        TEST(LineChartTest, EmptyDataRendersAMessageRatherThanACrashOrABlankBox) {
            app::Theme theme;
            LineChartData data;
            data.empty_message = "no runs yet";

            const std::string drawn =
                    testing::render_to_text(line_chart(data, theme, {.width = 60, .height = 6}), 60, 8);

            EXPECT_NE(drawn.find("no runs yet"), std::string::npos) << drawn;
        }

        TEST(LineChartTest, AVeryNarrowChartDegradesRatherThanOverflowing) {
            // Ten columns is narrower than the label gutter. The plot is what
            // survives; nothing may be written outside the box.
            app::Theme theme;
            LineChartData data;
            data.series = a_run();

            const std::string drawn =
                    testing::render_to_text(line_chart(data, theme, {.width = 10, .height = 4}), 10, 6);

            for (const std::string_view line: {std::string_view{drawn}}) {
                for (const char byte: line) {
                    EXPECT_NE(byte, '\t') << drawn;
                }
            }
            std::size_t longest = 0;
            std::size_t at = 0;
            while (at < drawn.size()) {
                const std::size_t end = drawn.find('\n', at);
                longest = std::max(longest, (end == std::string::npos ? drawn.size() : end) - at);
                at = end == std::string::npos ? drawn.size() : end + 1;
            }
            EXPECT_LE(longest, 10U) << "nothing spills past the box:\n" << drawn;
        }

        TEST(LineChartTest, SnapshotAt80Columns) {
            app::Theme theme;
            theme.name = "snapshot";
            LineChartData data;
            data.series = a_run();
            data.errors = {12.0, 41.0};

            testing::expect_matches_golden(
                    "line_chart_80",
                    testing::render_to_text(line_chart(data, theme, {.width = 78, .height = 8}), 80, 10));
        }

        TEST(LineChartTest, SnapshotAt120Columns) {
            app::Theme theme;
            theme.name = "snapshot";
            LineChartData data;
            data.series = a_run();
            data.errors = {12.0, 41.0};

            testing::expect_matches_golden(
                    "line_chart_120",
                    testing::render_to_text(line_chart(data, theme, {.width = 118, .height = 8}), 120, 10));
        }

    }  // namespace
}  // namespace typeit::tui
