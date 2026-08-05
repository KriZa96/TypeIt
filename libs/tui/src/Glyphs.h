// Every decorative character, twice (TI-085, UX §5).
//
// One Unicode form and one ASCII form for each, chosen by what the terminal
// can actually draw. This replaces `FTXUI_MICROSOFT_TERMINAL_FALLBACK`, a
// macro 1.0 branches on and **no build file ever defines** — so the Unicode
// branch always compiled and the fallback has never once executed on any
// machine.
//
// The enum is what makes "both sets define every glyph" a loop in a test
// rather than a table somebody keeps half-updated: a new glyph without an
// ASCII form fails to compile the switch, and one with an empty form fails the
// test.
#ifndef TYPEIT_TUI_GLYPHS_H
#define TYPEIT_TUI_GLYPHS_H

#include <array>
#include <cstdint>
#include <string_view>

#include "typeit/app/Capabilities.h"

namespace typeit::tui {

    enum class Glyph : std::uint8_t {
        BorderVertical,
        BorderHorizontal,
        BorderTopLeft,
        BorderTopRight,
        BorderBottomLeft,
        BorderBottomRight,
        RadioSelected,
        RadioEmpty,
        CaretBlock,
        CaretUnderline,
        ProgressFilled,
        ProgressEmpty,
        TrendUp,
        TrendDown,
        TrendFlat,
        Life,
    };

    inline constexpr std::array<Glyph, 16> kAllGlyphs{
            Glyph::BorderVertical,   Glyph::BorderHorizontal,  Glyph::BorderTopLeft,  Glyph::BorderTopRight,
            Glyph::BorderBottomLeft, Glyph::BorderBottomRight, Glyph::RadioSelected,  Glyph::RadioEmpty,
            Glyph::CaretBlock,       Glyph::CaretUnderline,    Glyph::ProgressFilled, Glyph::ProgressEmpty,
            Glyph::TrendUp,          Glyph::TrendDown,         Glyph::TrendFlat,      Glyph::Life,
    };

    /// The eight sparkline levels, lightest to heaviest. Its own thing rather
    /// than eight enumerators, because every caller wants the whole ramp and
    /// indexes into it.
    [[nodiscard]] std::string_view sparkline_level(app::GlyphSet set, std::size_t level);

    /// How many levels the ramp has. Both sets have the same number, so a
    /// caller scales once and draws either.
    inline constexpr std::size_t kSparklineLevels = 8;

    [[nodiscard]] std::string_view glyph(app::GlyphSet set, Glyph which);

    /// The set to draw with: the configuration if it has an opinion, otherwise
    /// what the terminal was detected to support.
    ///
    /// `auto` means "detect", which is why it is not an override. Anything
    /// else the configuration says wins — somebody who has told the program
    /// their font cannot draw box characters should not be argued with.
    [[nodiscard]] app::GlyphSet glyphs_for(std::string_view configured, app::GlyphSet detected);

}  // namespace typeit::tui

#endif  // TYPEIT_TUI_GLYPHS_H
