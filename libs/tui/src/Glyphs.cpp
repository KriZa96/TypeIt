#include "Glyphs.h"

#include <array>
#include <cstddef>
#include <string_view>

#include "typeit/app/Capabilities.h"

namespace typeit::tui {
    namespace {

        /// UX §5's table, in one place. A switch rather than two arrays: a new
        /// enumerator without a form here is a compile error, which is what
        /// stops the ASCII set quietly falling behind.
        struct Forms {
            std::string_view unicode;
            std::string_view ascii;
        };

        constexpr Forms forms_for(Glyph which) {
            switch (which) {
                case Glyph::BorderVertical:
                    return {.unicode = "│", .ascii = "|"};
                case Glyph::BorderHorizontal:
                    return {.unicode = "─", .ascii = "-"};
                case Glyph::BorderTopLeft:
                    return {.unicode = "┌", .ascii = "+"};
                case Glyph::BorderTopRight:
                    return {.unicode = "┐", .ascii = "+"};
                case Glyph::BorderBottomLeft:
                    return {.unicode = "└", .ascii = "+"};
                case Glyph::BorderBottomRight:
                    return {.unicode = "┘", .ascii = "+"};
                case Glyph::RadioSelected:
                    return {.unicode = "◉", .ascii = "(*)"};
                case Glyph::RadioEmpty:
                    return {.unicode = "○", .ascii = "( )"};
                case Glyph::CaretBlock:
                    return {.unicode = "█", .ascii = "#"};
                case Glyph::CaretUnderline:
                    return {.unicode = "▁", .ascii = "_"};
                case Glyph::ProgressFilled:
                    return {.unicode = "█", .ascii = "#"};
                case Glyph::ProgressEmpty:
                    return {.unicode = "░", .ascii = "."};
                case Glyph::TrendUp:
                    return {.unicode = "▲", .ascii = "^"};
                case Glyph::TrendDown:
                    return {.unicode = "▼", .ascii = "v"};
                case Glyph::TrendFlat:
                    return {.unicode = "=", .ascii = "="};
                case Glyph::Life:
                    return {.unicode = "♥", .ascii = "*"};
            }
            return {.unicode = "?", .ascii = "?"};
        }

        constexpr std::array<std::string_view, kSparklineLevels> kUnicodeSparkline{"▁", "▂", "▃", "▄",
                                                                                   "▅", "▆", "▇", "█"};
        constexpr std::array<std::string_view, kSparklineLevels> kAsciiSparkline{".", ":", "-", "=",
                                                                                 "+", "*", "%", "@"};

    }  // namespace

    std::string_view glyph(app::GlyphSet set, Glyph which) {
        const Forms forms = forms_for(which);
        return set == app::GlyphSet::Unicode ? forms.unicode : forms.ascii;
    }

    std::string_view sparkline_level(app::GlyphSet set, std::size_t level) {
        // Clamped rather than asserted: a sparkline scales a measurement, and
        // an off-by-one at the top of the range should draw the tallest bar
        // rather than end the program.
        const std::size_t at = level < kSparklineLevels ? level : kSparklineLevels - 1;
        return set == app::GlyphSet::Unicode ? kUnicodeSparkline.at(at) : kAsciiSparkline.at(at);
    }

    app::GlyphSet glyphs_for(std::string_view configured, app::GlyphSet detected) {
        if (configured == "unicode") {
            return app::GlyphSet::Unicode;
        }
        if (configured == "ascii") {
            return app::GlyphSet::Ascii;
        }
        // "auto", or anything the config layer let through. Detection wins,
        // because a configuration that says "detect" is not an override.
        return detected;
    }

}  // namespace typeit::tui
