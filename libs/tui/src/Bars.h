// The two strips above and below the text (TI-088, UX §3.2).
//
// Functions, not classes. Neither holds anything between frames — a stats bar
// is its numbers and a hint bar is the keymap — so a class would be a place to
// put state that must not exist.
//
// **Every field is fixed width.** A WPM going from 9 to 100 must not widen its
// field, because the text under the typist's fingers would reflow while they
// were reading it. That is the whole reason this is worth a file.
#ifndef TYPEIT_TUI_BARS_H
#define TYPEIT_TUI_BARS_H

#include <cstddef>
#include <ftxui/dom/elements.hpp>
#include <string>
#include <vector>

#include "Keymap.h"
#include "typeit/app/Capabilities.h"
#include "typeit/app/Theme.h"
#include "typeit/core/util/Units.h"

namespace typeit::tui {

    /// What the bar shows. Absent values are drawn as placeholders of the same
    /// width rather than omitted, so the layout does not shift when a run
    /// starts producing numbers.
    struct SessionStats {
        core::Wpm wpm{0.0};
        core::Accuracy accuracy{0.0};
        /// Seconds left, or elapsed for a mode with no end. Below zero means
        /// "no timer", which draws as blanks.
        std::int64_t seconds = -1;
        std::size_t words_done = 0;
        std::size_t words_total = 0;
    };

    /// Which fields the configuration asks for. A hidden field is absent, and
    /// what remains still balances — a bar with a hole in it would look like a
    /// bug.
    struct StatsBarOptions {
        bool show_wpm = true;
        bool show_accuracy = true;
        bool show_progress = true;
        app::ColorDepth depth = app::ColorDepth::TrueColor;
    };

    /// The numbers, in fixed-width fields.
    [[nodiscard]] ftxui::Element stats_bar(const SessionStats& stats, const app::Theme& theme,
                                           StatsBarOptions options = {});

    /// One key hint, as a label and the key that does it.
    struct Hint {
        Action action;
        std::string label;
    };

    /// The bindings that apply here, drawn from the keymap so a rebind shows up
    /// without anybody remembering to update a string.
    [[nodiscard]] ftxui::Element key_hint_bar(const std::vector<Hint>& hints, const Keymap& keymap,
                                              const app::Theme& theme,
                                              app::ColorDepth depth = app::ColorDepth::TrueColor);

    /// A number in a field of `width`, right-aligned and never wider than the
    /// field. Exposed because "does this field change width" is the property
    /// worth testing directly.
    [[nodiscard]] std::string fixed_width(const std::string& value, std::size_t width);

}  // namespace typeit::tui

#endif  // TYPEIT_TUI_BARS_H
