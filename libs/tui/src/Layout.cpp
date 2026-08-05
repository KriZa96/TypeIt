#include "Layout.h"

#include <algorithm>
#include <cstddef>
#include <string_view>

namespace typeit::tui {
    namespace {

        /// Columns kept clear either side of the text at the comfortable
        /// density. Enough to stop the text touching the frame; not so much
        /// that a narrow terminal loses half its width to margins.
        constexpr std::size_t kComfortableMargin = 4;
        constexpr std::size_t kCompactMargin = 1;

        /// Rows the bars and their spacing take. Subtracted before deciding how
        /// many lines of text fit, because a layout that assumed room for both
        /// and then found none would draw the text off the bottom.
        constexpr std::size_t kComfortableChrome = 6;
        constexpr std::size_t kCompactChrome = 3;

    }  // namespace

    Density density_from(std::string_view configured) {
        return configured == "compact" ? Density::Compact : Density::Comfortable;
    }

    Layout layout_for(TerminalSize size, std::size_t configured_width, std::size_t configured_lines, Density density) {
        Layout layout;

        if (size.columns < kMinimumSize.columns || size.rows < kMinimumSize.rows) {
            // Nothing sensible fits. TI-090's screen says so rather than
            // drawing something unreadable.
            layout.too_small = true;
            layout.text_columns = std::max<std::size_t>(1, size.columns);
            layout.lines_visible = 1;
            layout.show_stats_bar = false;
            layout.show_hint_bar = false;
            return layout;
        }

        const std::size_t margin = density == Density::Comfortable ? kComfortableMargin : kCompactMargin;
        const std::size_t chrome = density == Density::Comfortable ? kComfortableChrome : kCompactChrome;

        // Zero means "fit the terminal". A fixed value is honoured, and
        // clamped to what the terminal has — a 200-column setting on an
        // 80-column terminal is a request nobody can grant.
        const std::size_t available = size.columns - (2 * margin);
        layout.text_columns = configured_width == 0 ? available : std::min(configured_width, available);
        layout.text_columns = std::max<std::size_t>(1, layout.text_columns);

        // Centred, which is what makes a fixed width look deliberate rather
        // than left over.
        layout.text_left_margin = (size.columns - layout.text_columns) / 2;

        const std::size_t for_text = size.rows > chrome ? size.rows - chrome : 1;
        layout.lines_visible = configured_lines == 0 ? for_text : std::min(configured_lines, for_text);
        layout.lines_visible = std::max<std::size_t>(1, layout.lines_visible);

        // The hint bar is the first thing to go: somebody on a short terminal
        // needs the text and the numbers more than a reminder of which key
        // quits.
        layout.show_stats_bar = size.rows >= kMinimumSize.rows;
        layout.show_hint_bar = size.rows >= kMinimumSize.rows + 2;
        return layout;
    }

}  // namespace typeit::tui
