#include "RaceHud.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <ftxui/dom/elements.hpp>
#include <string>
#include <vector>

#include "Bars.h"
#include "ColorQuantizer.h"
#include "Glyphs.h"
#include "typeit/app/Capabilities.h"
#include "typeit/app/Theme.h"
#include "typeit/core/race/DifficultyController.h"
#include "typeit/core/util/Units.h"

namespace typeit::tui {
    namespace {

        constexpr std::size_t kWpmWidth = 3;
        constexpr std::size_t kAccuracyWidth = 5;
        constexpr std::size_t kLeadWidth = 4;
        /// Two ends and at least one column between them.
        constexpr std::size_t kMinimumBarWidth = 3;

        [[nodiscard]] std::string whole(double value) {
            // Truncated, as everywhere else: a speed that reads 100 when it is
            // 99.6 is a number somebody screenshots and argues about.
            return std::to_string(static_cast<std::int64_t>(value));
        }

        /// The lead, signed, because "-12" is information and "12" would be a
        /// lie about which way round the two are.
        [[nodiscard]] std::string lead_text(double lead) {
            const auto rounded = static_cast<std::int64_t>(lead);
            return rounded < 0 ? "-" + std::to_string(-rounded) : std::to_string(rounded);
        }

        [[nodiscard]] Glyph trend_glyph(core::RampTrend trend) {
            switch (trend) {
                case core::RampTrend::Climbing:
                    return Glyph::TrendUp;
                case core::RampTrend::BackingOff:
                    return Glyph::TrendDown;
                case core::RampTrend::Holding:
                    break;
            }
            return Glyph::TrendFlat;
        }

        /// Climbing is good news, backing off is bad, holding is neither — the
        /// colour says the same thing as the arrow, for anybody who reads one
        /// faster than the other.
        [[nodiscard]] app::ThemeColor trend_color(core::RampTrend trend) {
            switch (trend) {
                case core::RampTrend::Climbing:
                    return app::ThemeColor::Success;
                case core::RampTrend::BackingOff:
                    return app::ThemeColor::Warning;
                case core::RampTrend::Holding:
                    break;
            }
            return app::ThemeColor::Muted;
        }

        [[nodiscard]] ftxui::Element labelled(const std::string& label, const std::string& value,
                                              const app::Theme& theme, app::ColorDepth depth,
                                              app::ThemeColor value_color = app::ThemeColor::Accent) {
            return ftxui::hbox({
                    ftxui::text(label + " ") | ftxui::color(style_for(theme, app::ThemeColor::Muted, depth).color),
                    ftxui::text(value) | ftxui::color(style_for(theme, value_color, depth).color),
            });
        }

        /// `♥♥♡` — what is left and what has gone, so the total is visible
        /// rather than remembered.
        [[nodiscard]] ftxui::Element lives(const RaceHudState& state, const app::Theme& theme,
                                           const RaceHudOptions& options) {
            std::string full;
            for (std::size_t at = 0; at < state.lives_left; ++at) {
                full += glyph(options.glyphs, Glyph::Life);
            }
            std::string spent;
            for (std::size_t at = state.lives_left; at < state.lives_total; ++at) {
                spent += glyph(options.glyphs, Glyph::ProgressEmpty);
            }
            return ftxui::hbox({
                    ftxui::text(full) | ftxui::color(style_for(theme, app::ThemeColor::Error, options.depth).color),
                    ftxui::text(spent) | ftxui::color(style_for(theme, app::ThemeColor::Muted, options.depth).color),
            });
        }

    }  // namespace

    std::size_t bar_column(const RaceHudState& state, std::size_t width, core::GraphemeIndex at, std::size_t window) {
        if (width <= kMinimumBarWidth || window == 0) {
            return 0;
        }
        // The inside of the bar, between the two ends.
        const std::size_t inner = width - 2;

        // The window ends a little past whoever is further along, so neither
        // marker ever sits on the closing end where it would be mistaken for
        // it.
        const std::size_t furthest = std::max(state.player.value, state.pacer.value);
        const std::size_t margin = std::max<std::size_t>(1, window / 10);
        const std::size_t right = furthest + margin;
        const std::size_t left = right > window ? right - window : 0;

        if (at.value <= left) {
            return 0;
        }
        const double across = static_cast<double>(at.value - left) / static_cast<double>(right - left);
        const auto column = static_cast<std::size_t>(across * static_cast<double>(inner));
        return std::min(column, inner - 1);
    }

    ftxui::Element race_stats_bar(const RaceHudState& state, const app::Theme& theme, RaceHudOptions options) {
        const std::string accuracy = fixed_width(whole(state.accuracy.value * 100.0) + "%", kAccuracyWidth);
        return ftxui::hbox({
                labelled("target", fixed_width(whole(state.target.value), kWpmWidth) + " wpm", theme, options.depth),
                ftxui::text(" "),
                // The gate, made visible. A typist who is well ahead and gaining
                // nothing should be able to see that it is their accuracy
                // holding them, not wonder why the number stopped moving.
                ftxui::text(std::string{glyph(options.glyphs, trend_glyph(state.trend))}) |
                        ftxui::color(style_for(theme, trend_color(state.trend), options.depth).color),
                ftxui::text("   "),
                labelled("you", fixed_width(whole(state.you.value), kWpmWidth) + " wpm", theme, options.depth),
                ftxui::text("   "),
                labelled("acc", accuracy, theme, options.depth),
                ftxui::text("   "),
                labelled("lead", fixed_width(lead_text(state.lead), kLeadWidth), theme, options.depth,
                         state.lead > 0.0 ? app::ThemeColor::Accent : app::ThemeColor::Warning),
                ftxui::text("   "),
                lives(state, theme, options),
        });
    }

    ftxui::Element pacer_bar(const RaceHudState& state, const app::Theme& theme, std::size_t width,
                             RaceHudOptions options) {
        if (width < kMinimumBarWidth) {
            // Narrower than its own two ends. Nothing legible fits, and drawing
            // a partial bar would be worse than drawing none — the responsive
            // layout has a too-small screen for this (TI-090).
            return ftxui::text("");
        }
        const std::size_t inner = width - 2;
        const std::size_t ghost = bar_column(state, width, state.pacer, options.window);
        const std::size_t typist = bar_column(state, width, state.player, options.window);

        const Styling pacer_style = style_for(theme, app::ThemeColor::Pacer, options.depth);
        const Styling caret_style = style_for(theme, app::ThemeColor::Caret, options.depth);
        const Styling empty_style = style_for(theme, app::ThemeColor::Muted, options.depth);
        const Styling border_style = style_for(theme, app::ThemeColor::Border, options.depth);

        std::vector<ftxui::Element> columns;
        columns.reserve(width);
        columns.push_back(ftxui::text("|") | ftxui::color(border_style.color));
        for (std::size_t column = 0; column < inner; ++column) {
            if (column == typist) {
                // The typist wins the column when both land on it: being level
                // is the moment that matters, and losing the caret to the ghost
                // is losing the thing the eye is tracking.
                columns.push_back(ftxui::text(std::string{glyph(options.glyphs, Glyph::CaretBlock)}) |
                                  ftxui::color(caret_style.color));
            } else if (column <= ghost) {
                columns.push_back(ftxui::text(std::string{glyph(options.glyphs, Glyph::ProgressFilled)}) |
                                  ftxui::color(pacer_style.color));
            } else {
                columns.push_back(ftxui::text(std::string{glyph(options.glyphs, Glyph::ProgressEmpty)}) |
                                  ftxui::color(empty_style.color));
            }
        }
        columns.push_back(ftxui::text("|") | ftxui::color(border_style.color));
        return ftxui::hbox(std::move(columns));
    }

}  // namespace typeit::tui
