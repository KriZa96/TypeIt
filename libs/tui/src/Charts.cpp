#include "Charts.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <ftxui/dom/elements.hpp>
#include <string>
#include <utility>
#include <vector>

#include "ColorQuantizer.h"
#include "Glyphs.h"
#include "typeit/app/Theme.h"

namespace typeit::tui {
    namespace {

        /// The span of a series, with the empty case answered once rather than
        /// at every call site.
        struct Range {
            double low = 0.0;
            double high = 0.0;

            [[nodiscard]] bool flat() const noexcept { return high - low < 1e-9; }
        };

        Range range_of(const std::vector<double>& values) {
            if (values.empty()) {
                return {};
            }
            const auto [low, high] = std::ranges::minmax_element(values);
            return Range{.low = *low, .high = *high};
        }

        /// `values` reduced to exactly `cells` by averaging equal buckets.
        ///
        /// Averaging rather than sampling every nth: a spike dropped by a
        /// stride is a spike the typist never sees, and the whole point of a
        /// trend line is the shape.
        std::vector<double> resampled(const std::vector<double>& values, std::size_t cells) {
            if (values.size() <= cells || cells == 0) {
                return values;
            }
            std::vector<double> out;
            out.reserve(cells);
            for (std::size_t cell = 0; cell < cells; ++cell) {
                const std::size_t from = cell * values.size() / cells;
                const std::size_t to = std::max(from + 1, (cell + 1) * values.size() / cells);
                double total = 0.0;
                for (std::size_t at = from; at < to && at < values.size(); ++at) {
                    total += values.at(at);
                }
                out.push_back(total / static_cast<double>(to - from));
            }
            return out;
        }

        /// Negatives clamped away before anything scales them — see the header
        /// for why the whole series is not rejected instead.
        std::vector<double> without_negatives(const std::vector<double>& values) {
            std::vector<double> out;
            out.reserve(values.size());
            for (const double value: values) {
                out.push_back(std::max(0.0, value));
            }
            return out;
        }

        /// Which of the eight ramp levels a value sits at.
        std::size_t level_of(double value, const Range& span) {
            if (span.flat()) {
                // The middle, not the floor. A week of identical scores is a
                // flat line, and drawing it at the bottom says something false.
                return kSparklineLevels / 2;
            }
            const double fraction = (value - span.low) / (span.high - span.low);
            const auto level = static_cast<std::size_t>(std::lround(fraction * static_cast<double>(kSparklineLevels - 1)));
            return std::min(level, kSparklineLevels - 1);
        }

    }  // namespace

    std::string sparkline_text(const std::vector<double>& values, ChartOptions options) {
        if (values.empty()) {
            // Nothing, rather than a row of the lowest level: "no runs" and "a
            // run of zero" are different facts.
            return {};
        }
        const std::vector<double> cells = resampled(without_negatives(values), options.width);
        const Range span = range_of(cells);

        std::string out;
        for (const double value: cells) {
            out += sparkline_level(options.glyphs, level_of(value, span));
        }
        return out;
    }

    ftxui::Element sparkline(const std::vector<double>& values, const app::Theme& theme, ChartOptions options) {
        const Styling accent = style_for(theme, app::ThemeColor::Accent, options.depth);
        return ftxui::text(sparkline_text(values, options)) | ftxui::color(accent.color);
    }

    std::vector<std::size_t> bucket_counts(const std::vector<double>& values, std::size_t buckets) {
        if (values.empty() || buckets == 0) {
            return {};
        }
        std::vector<std::size_t> counts(buckets, 0);
        const Range span = range_of(values);
        if (span.flat()) {
            // Every value is the same one, so they are all in one bucket. Which
            // bucket is arbitrary; the first is the one a reader expects to see
            // a single bar in.
            counts.front() = values.size();
            return counts;
        }

        const double step = (span.high - span.low) / static_cast<double>(buckets);
        for (const double value: values) {
            const auto index = static_cast<std::size_t>((value - span.low) / step);
            // The top bucket is closed rather than half-open, so the maximum
            // lands in the last bucket instead of one past the end.
            counts.at(std::min(index, buckets - 1)) += 1;
        }
        return counts;
    }

    ftxui::Element histogram(const std::vector<double>& values, const app::Theme& theme, ChartOptions options) {
        if (values.empty() || options.width == 0 || options.height == 0) {
            return ftxui::text("");
        }

        // One bucket per two columns, so a bar and the gap beside it both fit,
        // and never more buckets than there are values to put in them.
        const std::size_t buckets = std::max<std::size_t>(1, std::min(options.width / 2, values.size()));
        const std::vector<std::size_t> counts = bucket_counts(values, buckets);
        const std::size_t tallest = *std::ranges::max_element(counts);

        const Styling accent = style_for(theme, app::ThemeColor::Accent, options.depth);
        const std::string_view filled = glyph(options.glyphs, Glyph::ProgressFilled);

        std::vector<ftxui::Element> rows;
        for (std::size_t row = options.height; row > 0; --row) {
            std::string line;
            for (const std::size_t count: counts) {
                // Scaled to the tallest bucket, so the shape fills the box
                // whatever the absolute counts are.
                // Rounded up, so a bucket with anything in it always draws at
                // least one cell — a bar rounded away is a bucket the reader
                // is told is empty.
                const std::size_t bar = tallest == 0 ? 0 : ((count * options.height) + tallest - 1) / tallest;
                line += bar >= row ? std::string{filled} : " ";
                line += ' ';
            }
            rows.push_back(ftxui::text(line) | ftxui::color(accent.color));
        }
        return ftxui::vbox(std::move(rows));
    }

    std::vector<double> axis_ticks(double low, double high, std::size_t wanted) {
        if (wanted == 0) {
            return {};
        }
        if (high - low < 1e-9) {
            // A flat series still gets an axis: one tick at the value it sits
            // at, rather than an empty gutter that looks like a rendering bug.
            return {low};
        }

        // 1, 2 or 5 times a power of ten — the intervals a person reads without
        // doing arithmetic. Anything else and the labels are technically evenly
        // spaced and practically unreadable.
        const double rough = (high - low) / static_cast<double>(wanted);
        const double magnitude = std::pow(10.0, std::floor(std::log10(rough)));
        const double normalised = rough / magnitude;

        double nice = 10.0;
        for (const double candidate: {1.0, 2.0, 5.0}) {
            if (normalised <= candidate) {
                nice = candidate;
                break;
            }
        }
        const double step = magnitude * nice;

        // Counted rather than accumulated: adding `step` to itself two hundred
        // times drifts, and a tick at 99.99999 prints as 99 beside one at 100.
        const double first = std::ceil(low / step) * step;
        std::vector<double> ticks;
        for (std::size_t at = 0; at <= wanted * 4; ++at) {
            const double tick = first + (step * static_cast<double>(at));
            if (tick > high + (step * 0.001)) {
                break;
            }
            ticks.push_back(tick);
        }
        return ticks;
    }

    namespace {

        /// The three marks the plot is drawn with. Named, because the row
        /// colouring below is a lookup on them and a stray literal there would
        /// be a colour that silently stops matching its glyph.
        constexpr char kSeriesMark = '*';
        constexpr char kOverlayMark = '-';
        constexpr char kErrorMark = 'x';

        /// The gutter the y labels live in. A constant rather than a
        /// parameter: a label and the width it is padded to are both numbers,
        /// and two numbers in a row are two numbers somebody swaps.
        constexpr std::size_t kGutter = 5;

        /// A label padded to the gutter, so the plot area starts at the same
        /// column on every row.
        std::string tick_label(double value) {
            std::string text = std::to_string(static_cast<std::int64_t>(value));
            if (text.size() < kGutter) {
                text.insert(0, kGutter - text.size(), ' ');
            }
            return text;
        }

        /// Which column an `x` falls in. Clamped, because a marker for an event
        /// one millisecond past the last sample belongs on the last column
        /// rather than off the end of the row.
        std::size_t column_of(double x, const Range& span, std::size_t width) {
            if (width == 0) {
                return 0;
            }
            if (span.flat()) {
                return 0;
            }
            const double fraction = (x - span.low) / (span.high - span.low);
            const auto column = static_cast<std::size_t>(std::lround(fraction * static_cast<double>(width - 1)));
            return std::min(column, width - 1);
        }

        Range span_of(const std::vector<Point>& points, bool on_x) {
            if (points.empty()) {
                return {};
            }
            std::vector<double> values;
            values.reserve(points.size());
            for (const Point& point: points) {
                values.push_back(on_x ? point.x : point.y);
            }
            return range_of(values);
        }

    }  // namespace

    namespace {

        /// The plot area as characters, one row per line, top row first.
        ///
        /// Marks rather than colours, so the colouring below is a lookup and
        /// this is the only place that knows where a point goes.
        /// Both spans together. A pair of `Range`s side by side in a
        /// parameter list is a pair somebody eventually passes the wrong way
        /// round, and a chart with its axes swapped still draws.
        struct Axes {
            Range x;
            Range y;
        };

        std::vector<std::string> plotted(const LineChartData& data, const Axes& axes, std::size_t width,
                                         std::size_t height) {
            std::vector<std::string> grid(height, std::string(width, ' '));

            const auto draw = [&](const std::vector<Point>& points, char mark) {
                for (const Point& point: points) {
                    const double fraction = (point.y - axes.y.low) / (axes.y.high - axes.y.low);
                    const auto from_bottom = static_cast<std::size_t>(std::clamp(fraction, 0.0, 1.0) *
                                                                      static_cast<double>(height - 1));
                    grid.at(height - 1 - from_bottom).at(column_of(point.x, axes.x, width)) = mark;
                }
            };
            draw(data.series, kSeriesMark);
            draw(data.overlay, kOverlayMark);

            // Markers last, and in the series' own column, so an error sits
            // *on* the line at the moment it happened. A row of them along the
            // bottom would be a second chart sharing an axis, which is how a
            // reader ends up matching peaks to marks by eye.
            for (const double at: data.errors) {
                const std::size_t column = column_of(at, axes.x, width);
                for (std::size_t row = 0; row < height; ++row) {
                    if (grid.at(row).at(column) == kSeriesMark) {
                        grid.at(row).at(column) = kErrorMark;
                        break;
                    }
                }
            }
            return grid;
        }

        /// One row, split into runs of identical marks.
        ///
        /// One element per run rather than one per row, so the pacer and the
        /// error markers keep their own colours. A row in a single colour would
        /// make them one picture at `Mono`, where the glyph is all there is —
        /// and it is the glyph that has to carry it there anyway.
        ftxui::Element coloured_row(const std::string& line, const app::Theme& theme, app::ColorDepth depth) {
            const auto colour_for = [&](char mark) {
                if (mark == kErrorMark) {
                    return style_for(theme, app::ThemeColor::Error, depth).color;
                }
                if (mark == kOverlayMark) {
                    return style_for(theme, app::ThemeColor::Pacer, depth).color;
                }
                return style_for(theme, app::ThemeColor::Accent, depth).color;
            };

            std::vector<ftxui::Element> runs;
            for (std::size_t at = 0; at < line.size();) {
                const std::size_t end = line.find_first_not_of(line.at(at), at);
                const std::size_t stop = end == std::string::npos ? line.size() : end;
                runs.push_back(ftxui::text(line.substr(at, stop - at)) | ftxui::color(colour_for(line.at(at))));
                at = stop;
            }
            return ftxui::hbox(std::move(runs));
        }

    }  // namespace

    ftxui::Element line_chart(const LineChartData& data, const app::Theme& theme, ChartOptions options) {
        const Styling muted = style_for(theme, app::ThemeColor::Muted, options.depth);
        const Styling pacer = style_for(theme, app::ThemeColor::Pacer, options.depth);

        if (data.series.empty()) {
            // A sentence, not a crash and not a blank box: an empty chart that
            // looks like a broken chart is worse than either.
            return ftxui::vbox({ftxui::text(data.empty_message) | ftxui::color(muted.color)});
        }

        // The gutter holds the y labels; whatever is left is the plot. A
        // terminal too narrow for both draws the plot and drops the labels,
        // which is the half worth keeping.
        const std::size_t plot_width = options.width > kGutter + 1 ? options.width - kGutter - 1 : options.width;
        const std::size_t plot_height = std::max<std::size_t>(1, options.height);

        Range y = span_of(data.series, false);
        if (y.flat()) {
            // A flat series would otherwise be a line on the top edge with no
            // axis to read it against. Pad it into a range.
            y.low = std::max(0.0, y.low - 1.0);
            y.high += 1.0;
        }
        const std::vector<std::string> grid =
                plotted(data, Axes{.x = span_of(data.series, true), .y = y}, plot_width, plot_height);

        const std::vector<double> ticks = axis_ticks(y.low, y.high, plot_height);
        // Half a row's worth of value: a tick is labelled on the row it is
        // nearest to, and no row claims two.
        const double tolerance = (y.high - y.low) / static_cast<double>(plot_height * 2);

        std::vector<ftxui::Element> rows;
        rows.reserve(plot_height + 2);
        for (std::size_t row = 0; row < plot_height; ++row) {
            const double at_this_row =
                    y.high - ((y.high - y.low) * static_cast<double>(row) / static_cast<double>(plot_height));
            const bool labelled = std::ranges::any_of(
                    ticks, [&](double tick) { return std::abs(tick - at_this_row) < tolerance; });

            rows.push_back(ftxui::hbox({
                    ftxui::text(labelled ? tick_label(at_this_row) : std::string(kGutter, ' ')) |
                            ftxui::color(muted.color),
                    ftxui::text("|") | ftxui::color(muted.color),
                    coloured_row(grid.at(row), theme, options.depth),
            }));
        }

        rows.push_back(ftxui::hbox({
                ftxui::text(std::string(kGutter, ' ')),
                ftxui::text("+" + std::string(plot_width, '-')) | ftxui::color(muted.color),
        }));
        if (!data.overlay.empty()) {
            rows.push_back(ftxui::text("      - pacer") | ftxui::color(pacer.color));
        }
        return ftxui::vbox(std::move(rows));
    }

}  // namespace typeit::tui
