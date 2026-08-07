#include "Heatmap.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <ftxui/dom/elements.hpp>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "ColorQuantizer.h"
#include "typeit/app/Theme.h"

namespace typeit::tui {
    namespace {

        /// The staggered offsets of the three letter rows, in half-widths
        /// doubled to whole cells. A grid drawn flush left is a grid, not a
        /// keyboard, and the whole point is that a typist recognises their own
        /// hands in it.
        constexpr std::array<std::size_t, 4> kRowIndent{0, 1, 2, 4};

        /// Where the ramp changes. Five bands rather than a continuous scale:
        /// a typist acts on "this key is bad", not on the third decimal of its
        /// error rate, and five colours are five decisions.
        constexpr std::array<double, 4> kBands{0.02, 0.05, 0.10, 0.20};

        std::size_t band_of(double rate) {
            std::size_t band = 0;
            while (band < kBands.size() && rate >= kBands.at(band)) {
                ++band;
            }
            return band;
        }

        app::ThemeColor colour_of(std::size_t band) {
            switch (band) {
                case 0:
                    return app::ThemeColor::Success;
                case 1:
                case 2:
                    return app::ThemeColor::Warning;
                default:
                    return app::ThemeColor::Error;
            }
        }

    }  // namespace

    const std::vector<std::string>& qwerty_rows() {
        // Punctuation and space included: a typist who misses every comma
        // learns nothing from a heatmap that only knows about letters, and the
        // stats have the data either way.
        static const std::vector<std::string> rows{"1234567890-=", "qwertyuiop[]", "asdfghjkl;'", "zxcvbnm,./"};
        return rows;
    }

    std::optional<double> error_rate(const core::KeyStat& stat) {
        if (stat.attempts == 0) {
            return std::nullopt;
        }
        return static_cast<double>(stat.errors) / static_cast<double>(stat.attempts);
    }

    std::string_view intensity_glyph(double rate, app::GlyphSet set) {
        // Denser as it gets worse, in both sets, so `Mono` says the same thing
        // the colours do. The two ramps are the same length, which is what
        // makes the monotonicity test one loop rather than two.
        static constexpr std::array<std::string_view, 5> kUnicode{" ", "░", "▒", "▓", "█"};
        static constexpr std::array<std::string_view, 5> kAscii{" ", ".", "+", "*", "#"};
        const std::size_t band = band_of(rate);
        return set == app::GlyphSet::Ascii ? kAscii.at(band) : kUnicode.at(band);
    }

    ftxui::Element heatmap(const core::KeyStats& stats, const app::Theme& theme, HeatmapOptions options) {
        const Styling muted = style_for(theme, app::ThemeColor::Muted, options.depth);
        const bool mono = options.depth == app::ColorDepth::Mono;

        std::vector<ftxui::Element> rows;
        for (std::size_t line = 0; line < qwerty_rows().size(); ++line) {
            std::vector<ftxui::Element> cells{ftxui::text(std::string(kRowIndent.at(line), ' '))};
            for (const char letter: qwerty_rows().at(line)) {
                const std::string key{letter};
                const auto found = stats.per_grapheme.find(key);
                const std::optional<double> rate =
                        found == stats.per_grapheme.end() ? std::nullopt : error_rate(found->second);

                if (!rate.has_value()) {
                    // Never pressed. Drawn dim and marked, rather than blank,
                    // so it reads as "nothing recorded" and not as "perfect" —
                    // colouring it like a clean key would send somebody off to
                    // practise the keys they already type. The mark is not one
                    // of the intensity ramp's, in either set, so it cannot be
                    // mistaken for the lightest band.
                    const std::string_view unknown = options.glyphs == app::GlyphSet::Ascii ? "?" : "·";
                    cells.push_back(ftxui::text(key + std::string{unknown} + " ") | ftxui::color(muted.color));
                    continue;
                }

                const std::size_t band = band_of(*rate);
                const Styling styling = style_for(theme, colour_of(band), options.depth);
                // In `Mono` the glyph is all there is, so it carries the
                // intensity; in colour it is drawn as well, because a
                // colour-blind reader is in the same position.
                const std::string cell = key + std::string{intensity_glyph(*rate, options.glyphs)} + " ";
                cells.push_back(mono ? ftxui::text(cell) : ftxui::text(cell) | ftxui::color(styling.color));
            }
            rows.push_back(ftxui::hbox(std::move(cells)));
        }
        return ftxui::vbox(std::move(rows));
    }

}  // namespace typeit::tui
