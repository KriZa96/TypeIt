// The race strip and the pacer bar (TI-125, UX §3.3).
//
// The pacer bar is the primary feedback channel: the filled section is the
// ghost, the caret is the typist, and the gap between them *is* the lead. It is
// the one place a race is legible at a glance, so it gets its own file rather
// than a third mode in `Bars.h`.
//
// The trend marker beside the target speed is what makes the accuracy gate
// visible. Without it a typist forty graphemes ahead and gaining nothing has no
// way to tell whether they are in the dead band or being held back by their
// accuracy, and "the speed stopped going up" becomes a mystery rather than a
// message (GAMEPLAY §3.2).
//
// Functions, not a class: nothing here is held between frames, and a class
// would be somewhere to put state that must not exist.
#ifndef TYPEIT_TUI_RACEHUD_H
#define TYPEIT_TUI_RACEHUD_H

#include <cstddef>
#include <ftxui/dom/elements.hpp>

#include "typeit/app/Capabilities.h"
#include "typeit/app/Theme.h"
#include "typeit/core/race/DifficultyController.h"
#include "typeit/core/util/Units.h"

namespace typeit::tui {

    /// Everything the strip and the bar draw. A snapshot of one frame, passed
    /// whole so the two cannot disagree about which frame they are drawing.
    struct RaceHudState {
        /// The ghost's speed, and which way the ramp is taking it.
        core::Wpm target{0.0};
        core::RampTrend trend = core::RampTrend::Holding;

        /// The typist's rolling speed and accuracy — the same numbers the ramp
        /// is reading, so the HUD explains the ramp rather than describing
        /// something adjacent to it.
        core::Wpm you{0.0};
        core::Accuracy accuracy{0.0};

        /// Graphemes. Negative while the ghost is ahead, which is a state the
        /// run survives for the length of the grace window.
        double lead = 0.0;

        std::size_t lives_left = 0;
        std::size_t lives_total = 0;

        core::GraphemeIndex pacer{0};
        core::GraphemeIndex player{0};
    };

    struct RaceHudOptions {
        app::ColorDepth depth = app::ColorDepth::TrueColor;
        app::GlyphSet glyphs = app::GlyphSet::Unicode;
        /// How many graphemes the pacer bar spans.
        ///
        /// A window rather than the whole text: over a three-thousand-grapheme
        /// chapter a lead of thirty is a fifth of one column, which is not a
        /// feedback channel. A hundred graphemes makes a comfortable lead about
        /// a quarter of the bar, which is a gap somebody can read without
        /// looking at the number.
        std::size_t window = 100;
    };

    /// `target 74 wpm ▲   you 81 wpm   98.1%   lead 31   ♥♥`
    [[nodiscard]] ftxui::Element race_stats_bar(const RaceHudState& state, const app::Theme& theme,
                                                RaceHudOptions options = {});

    /// `pacer ├──────▓▓▓▓▓░░░░░░░░┤`, `width` columns wide including the ends.
    ///
    /// The filled run ends where the ghost is and the caret sits where the
    /// typist is, so the distance between them is the lead drawn to scale.
    [[nodiscard]] ftxui::Element pacer_bar(const RaceHudState& state, const app::Theme& theme, std::size_t width,
                                           RaceHudOptions options = {});

    /// Which column of a `width`-wide bar a grapheme position falls in.
    ///
    /// Exposed because it is the whole geometry of the bar, and a test that
    /// re-derived it would be asserting its own arithmetic rather than the
    /// widget's.
    [[nodiscard]] std::size_t bar_column(const RaceHudState& state, std::size_t width, core::GraphemeIndex at,
                                         std::size_t window);

}  // namespace typeit::tui

#endif  // TYPEIT_TUI_RACEHUD_H
