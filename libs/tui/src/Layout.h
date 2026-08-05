// Where things go, computed from the terminal (TI-089, UX §6.4).
//
// 1.0 hardcodes `WIDTH = 75`, `HEIGHT = 10` and a 55-column wrap, which makes
// the interface unusable below about eighty columns and ignores resize
// entirely. Everything here is a function of the size the terminal reports, so
// a resize is answered by calling it again.
//
// A pure function over a size, deliberately: no state, so a resize cannot
// leave a stale value behind, and every case is a value in a test rather than
// a terminal somebody has to drag the corner of.
#ifndef TYPEIT_TUI_LAYOUT_H
#define TYPEIT_TUI_LAYOUT_H

#include <cstddef>
#include <cstdint>
#include <string_view>

namespace typeit::tui {

    struct TerminalSize {
        std::size_t columns = 80;
        std::size_t rows = 24;
    };

    /// UX §6.1's floor. Below this there is nowhere to put a line of text and
    /// a stats bar, so `TerminalTooSmallScreen` (TI-090) says so instead of
    /// drawing something unreadable.
    inline constexpr TerminalSize kMinimumSize{.columns = 40, .rows = 10};

    /// `comfortable` spends rows on breathing room; `compact` does not. The
    /// difference is documented in UX §3 and is the only thing the density
    /// setting changes.
    enum class Density : std::uint8_t {
        Compact,
        Comfortable,
    };

    [[nodiscard]] Density density_from(std::string_view configured);

    struct Layout {
        /// Columns the text wraps at, and where it starts so it sits centred.
        std::size_t text_columns = 0;
        std::size_t text_left_margin = 0;
        /// Lines of the text shown at once.
        std::size_t lines_visible = 3;
        /// Whether there is room for the bars at all. Below a certain height
        /// the text is what matters.
        bool show_stats_bar = true;
        bool show_hint_bar = true;
        /// Whether the terminal is big enough to use at all.
        bool too_small = false;
    };

    /// The layout for a terminal of this size.
    ///
    /// `configured_width` of zero means "fit the terminal", which is the
    /// setting most people want and the one a naive range check rejects. A
    /// fixed value is honoured and centred, and is clamped to what the
    /// terminal actually has — a 200-column setting on an 80-column terminal
    /// is a request nobody can grant.
    [[nodiscard]] Layout layout_for(TerminalSize size, std::size_t configured_width, std::size_t configured_lines,
                                    Density density);

}  // namespace typeit::tui

#endif  // TYPEIT_TUI_LAYOUT_H
