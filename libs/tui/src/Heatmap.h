// The keyboard, coloured by how often each key goes wrong (TI-102, UX §3.5).
//
// This is the payoff for recording per-key statistics: thousands of keystrokes
// become one glance that says which fingers to work on. Nothing else in the
// program turns the `key_stat` table into an answer.
//
// **"No data" is not "no errors".** A key the typist has never pressed and a
// key they have never missed are opposite facts, and colouring both of them
// green would recommend practising exactly the wrong things. They are drawn
// differently and there is a test that says so.
#ifndef TYPEIT_TUI_HEATMAP_H
#define TYPEIT_TUI_HEATMAP_H

#include <cstddef>
#include <ftxui/dom/elements.hpp>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "typeit/app/Capabilities.h"
#include "typeit/app/Theme.h"
#include "typeit/core/metrics/KeyStats.h"

namespace typeit::tui {

    struct HeatmapOptions {
        app::GlyphSet glyphs = app::GlyphSet::Unicode;
        app::ColorDepth depth = app::ColorDepth::TrueColor;
    };

    /// The QWERTY rows, staggered as the keys physically are. Exposed so a test
    /// can assert the arrangement without reading it back out of a picture, and
    /// so a second layout later is another table rather than another function.
    [[nodiscard]] const std::vector<std::string>& qwerty_rows();

    /// The error rate for one key: errors over attempts, or absent when the key
    /// has never been pressed.
    ///
    /// `std::optional` rather than a sentinel rate, because every sentinel here
    /// is a real value: -1 is not a rate, and 0 is the *best possible* one.
    [[nodiscard]] std::optional<double> error_rate(const core::KeyStat& stat);

    /// The keyboard, one cell per key.
    ///
    /// At `Mono` the intensity is carried by the glyph rather than the colour —
    /// a heatmap with no heat is a picture of a keyboard.
    [[nodiscard]] ftxui::Element heatmap(const core::KeyStats& stats, const app::Theme& theme,
                                         HeatmapOptions options = {});

    /// The character drawn beside a key at the given error rate, or a space
    /// when there is no data. The `Mono` ramp, and the thing worth asserting
    /// monotonicity on.
    [[nodiscard]] std::string_view intensity_glyph(double rate, app::GlyphSet set);

}  // namespace typeit::tui

#endif  // TYPEIT_TUI_HEATMAP_H
