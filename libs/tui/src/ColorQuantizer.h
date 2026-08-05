// One theme, every terminal (TI-084, UX §4).
//
// Themes are authored in truecolor and nothing else. This maps a colour down
// to the xterm-256 cube, then to the sixteen ANSI colours, and finally to no
// colour at all — so an author writes one file and it works everywhere. 1.0
// hardcodes 256-palette entries (`Color::Grey82`, `Color::Salmon1`) with no
// fallback whatever, which is why it looks wrong on a 16-colour terminal and
// unreadable on a monochrome one.
//
// At `Mono` there is nothing left to say with colour, so state is said with
// attributes instead: bold, underline, reverse. That is what makes TypeIt
// usable over a serial console and under `NO_COLOR`.
#ifndef TYPEIT_TUI_COLORQUANTIZER_H
#define TYPEIT_TUI_COLORQUANTIZER_H

#include <cstdint>
#include <ftxui/screen/color.hpp>

#include "typeit/app/Capabilities.h"
#include "typeit/app/Theme.h"

namespace typeit::tui {

    /// How to draw one semantic colour at a given depth.
    ///
    /// Both halves, always: at `Mono` the colour is the terminal's default and
    /// the attributes carry the meaning, and at `TrueColor` the attributes are
    /// empty and the colour carries it. A caller applies both and does not
    /// have to know which depth it is on.
    struct Styling {
        ftxui::Color color = ftxui::Color::Default;
        bool bold = false;
        bool underline = false;
        bool inverted = false;
    };

    /// The colour, quantised. `TrueColor` returns it unchanged.
    [[nodiscard]] ftxui::Color quantize(app::Rgb color, app::ColorDepth depth);

    /// The xterm-256 index a colour maps to: the 6×6×6 cube, or the 24-step
    /// grey ramp when the channels are close enough to be grey. Exposed
    /// because it is the part worth checking against a reference table.
    [[nodiscard]] std::uint8_t xterm256_index(app::Rgb color);

    /// The ANSI index (0–15) a colour maps to. Perceptually ordered: a darker
    /// colour never maps to a lighter one.
    [[nodiscard]] std::uint8_t ansi16_index(app::Rgb color);

    /// How a semantic colour should be drawn on this terminal.
    ///
    /// At `Mono` the answer is attributes: the four typing states have to be
    /// told apart, and underline, reverse and bold are the only three ways
    /// left. Everything else is drawn plain.
    [[nodiscard]] Styling style_for(const app::Theme& theme, app::ThemeColor which, app::ColorDepth depth);

    /// Relative luminance, 0–255, by the usual weighting. The ordering the
    /// quantiser preserves is this one.
    [[nodiscard]] std::uint8_t luminance(app::Rgb color);

}  // namespace typeit::tui

#endif  // TYPEIT_TUI_COLORQUANTIZER_H
