#include "Bars.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <ftxui/dom/elements.hpp>
#include <string>
#include <utility>
#include <vector>

#include "ColorQuantizer.h"
#include "Keymap.h"
#include "typeit/app/Theme.h"

namespace typeit::tui {
    namespace {

        /// Wide enough for the widest value each field can hold: `999` WPM,
        /// `100%`, `59:59`, `999/999`. Named rather than inline, because the
        /// whole point is that they never change.
        constexpr std::size_t kWpmWidth = 3;
        constexpr std::size_t kAccuracyWidth = 4;
        constexpr std::size_t kTimeWidth = 5;
        constexpr std::size_t kProgressWidth = 7;

        constexpr std::int64_t kSecondsPerMinute = 60;

        ftxui::Element labelled(const std::string& label, const std::string& value, const app::Theme& theme,
                                app::ColorDepth depth) {
            const Styling muted = style_for(theme, app::ThemeColor::Muted, depth);
            const Styling accent = style_for(theme, app::ThemeColor::Accent, depth);
            return ftxui::hbox({
                    ftxui::text(label + " ") | ftxui::color(muted.color),
                    ftxui::text(value) | ftxui::color(accent.color),
            });
        }

        std::string whole(double value) {
            // Truncated rather than rounded: a WPM that reads 100 when it is
            // 99.6 is a WPM somebody will screenshot and argue about.
            return std::to_string(static_cast<std::int64_t>(value));
        }

    }  // namespace

    std::string fixed_width(const std::string& value, std::size_t width) {
        if (value.size() >= width) {
            // Truncated from the left, so the significant digits survive: a
            // field showing `999` for 1999 is wrong, but showing `199` is
            // worse.
            return value.substr(value.size() - width);
        }
        return std::string(width - value.size(), ' ') + value;
    }

    ftxui::Element stats_bar(const SessionStats& stats, const app::Theme& theme, StatsBarOptions options) {
        std::vector<ftxui::Element> fields;

        if (options.show_wpm) {
            fields.push_back(labelled("wpm", fixed_width(whole(stats.wpm.value), kWpmWidth), theme, options.depth));
        }
        if (options.show_accuracy) {
            const std::string percent = whole(stats.accuracy.value * 100.0) + "%";
            fields.push_back(labelled("acc", fixed_width(percent, kAccuracyWidth), theme, options.depth));
        }
        if (options.show_progress) {
            // A timer when there is one, a word count otherwise, and blanks
            // when there is neither — the same width in all three cases.
            std::string progress;
            if (stats.seconds >= 0) {
                const std::string seconds = std::to_string(stats.seconds % kSecondsPerMinute);
                progress = std::to_string(stats.seconds / kSecondsPerMinute) + ":" +
                           (seconds.size() < 2 ? "0" + seconds : seconds);
                fields.push_back(labelled("time", fixed_width(progress, kTimeWidth), theme, options.depth));
            } else if (stats.words_total > 0) {
                progress = std::to_string(stats.words_done) + "/" + std::to_string(stats.words_total);
                fields.push_back(labelled("words", fixed_width(progress, kProgressWidth), theme, options.depth));
            }
        }

        if (fields.empty()) {
            // Everything hidden. An empty line rather than a missing one, so
            // the layout below it does not move.
            return ftxui::text("");
        }

        // Separated rather than spaced apart: the fields are already fixed
        // width, so a fixed separator keeps the whole bar fixed too.
        std::vector<ftxui::Element> spaced;
        for (std::size_t at = 0; at < fields.size(); ++at) {
            if (at > 0) {
                spaced.push_back(ftxui::text("   "));
            }
            spaced.push_back(std::move(fields.at(at)));
        }
        return ftxui::hbox(std::move(spaced));
    }

    ftxui::Element key_hint_bar(const std::vector<Hint>& hints, const Keymap& keymap, const app::Theme& theme,
                                app::ColorDepth depth) {
        const Styling muted = style_for(theme, app::ThemeColor::Muted, depth);
        const Styling accent = style_for(theme, app::ThemeColor::Accent, depth);

        std::vector<ftxui::Element> cells;
        for (const Hint& hint: hints) {
            if (!cells.empty()) {
                cells.push_back(ftxui::text("  "));
            }
            // Read from the keymap, so a rebind shows here without anybody
            // remembering to update a string.
            cells.push_back(ftxui::text(to_string(keymap.binding(hint.action))) | ftxui::color(accent.color));
            cells.push_back(ftxui::text(" " + hint.label) | ftxui::color(muted.color));
        }

        if (cells.empty()) {
            return ftxui::text("");
        }
        return ftxui::hbox(std::move(cells));
    }

}  // namespace typeit::tui
