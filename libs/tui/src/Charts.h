// Numbers as pictures (TI-099 – TI-101, UX §3.4, §3.5).
//
// Three widgets in one file, for the same reason `Bars.h` holds two: none of
// them holds anything between frames, so each is a function over its data, and
// a file per function would be three headers that always change together.
//
// **Every one of them survives its degenerate input.** Empty, one value, all
// values equal, more values than there are cells — these are what a history
// widget actually meets on somebody's first day, and each one is a division by
// zero or an empty range waiting to happen. They are the tests.
#ifndef TYPEIT_TUI_CHARTS_H
#define TYPEIT_TUI_CHARTS_H

#include <cstddef>
#include <ftxui/dom/elements.hpp>
#include <string>
#include <vector>

#include "typeit/app/Capabilities.h"
#include "typeit/app/Theme.h"

namespace typeit::tui {

    /// What every chart here needs to draw itself.
    struct ChartOptions {
        /// Cells across. A chart wider than its data is padded, never stretched
        /// — a sparkline of three runs drawn across forty columns would imply
        /// thirty-seven runs nobody made.
        std::size_t width = 40;
        /// Rows of plot area, ignored by the sparkline, which is one row by
        /// definition.
        std::size_t height = 8;
        app::GlyphSet glyphs = app::GlyphSet::Unicode;
        app::ColorDepth depth = app::ColorDepth::TrueColor;
    };

    /// One row of `▁▂▃▄▅▆▇█`, or `.:-=+*%@` where that is all the terminal can
    /// draw.
    ///
    /// The documented rules for the awkward inputs:
    ///
    /// - **Empty** draws nothing at all — not a row of the lowest level, which
    ///   would read as "a run of zeroes" rather than "no runs".
    /// - **All equal** draws the *middle* level, not the lowest. A week of
    ///   identical scores is a flat line, and drawing it at the floor says
    ///   something false about it.
    /// - **Negative** values are clamped to zero. Nothing here can legitimately
    ///   be negative — WPM and accuracy are both floors at zero — so a negative
    ///   is bad data, and rejecting the whole series over one would lose the
    ///   rest of it.
    /// - **More values than cells** are averaged in equal buckets, so the shape
    ///   survives downsampling and the same input always gives the same
    ///   picture.
    [[nodiscard]] ftxui::Element sparkline(const std::vector<double>& values, const app::Theme& theme,
                                           ChartOptions options = {});

    /// The same ramp as a string, without the styling. Exposed because "how
    /// many cells, and which levels" is the property worth asserting, and
    /// digging it out of a rendered `Element` proves less.
    [[nodiscard]] std::string sparkline_text(const std::vector<double>& values, ChartOptions options = {});

    /// How many values fall in each bucket, over `[low, high]` split evenly.
    ///
    /// Half-open buckets — `[low, low + step)` and so on — with the last one
    /// closed at the top, so a value exactly on a boundary lands in exactly one
    /// bucket and the maximum is not silently dropped.
    [[nodiscard]] std::vector<std::size_t> bucket_counts(const std::vector<double>& values, std::size_t buckets);

    /// A distribution as vertical bars, tallest bucket at full height.
    ///
    /// The bucket count adapts to the width rather than being asked for: a
    /// histogram with more buckets than columns cannot draw them, and one with
    /// far fewer wastes the space it was given.
    [[nodiscard]] ftxui::Element histogram(const std::vector<double>& values, const app::Theme& theme,
                                           ChartOptions options = {});

    /// A point on the line chart. `x` is whatever the caller is plotting
    /// against — seconds into a run, or a run's index in a history.
    struct Point {
        double x = 0.0;
        double y = 0.0;
    };

    /// Everything the chart draws, so adding a second series later is a field
    /// rather than a fourth overload.
    struct LineChartData {
        std::vector<Point> series;
        /// Drawn in the pacer colour, over the top. The race pacer curve, and
        /// nothing else so far.
        std::vector<Point> overlay;
        /// `x` positions to mark with an error glyph. Their `y` is the series'
        /// own, so a marker sits on the line rather than beside it.
        std::vector<double> errors;
        std::string empty_message = "nothing to plot yet";
    };

    /// Ticks for an axis spanning `low`..`high`, at intervals a person reads
    /// without doing arithmetic: 1, 2, 5 and their powers of ten.
    ///
    /// Exposed because "are these readable numbers" is exactly the assertion,
    /// and it is one a test can make about a vector far more clearly than about
    /// a picture.
    [[nodiscard]] std::vector<double> axis_ticks(double low, double high, std::size_t wanted);

    /// The per-second chart, with axes and labels.
    [[nodiscard]] ftxui::Element line_chart(const LineChartData& data, const app::Theme& theme,
                                            ChartOptions options = {});

}  // namespace typeit::tui

#endif  // TYPEIT_TUI_CHARTS_H
