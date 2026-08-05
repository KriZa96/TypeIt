#include "screens/ResultsScreen.h"

#include <cstdint>
#include <ftxui/component/event.hpp>
#include <ftxui/dom/elements.hpp>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "Bars.h"
#include "ColorQuantizer.h"
#include "Keymap.h"
#include "typeit/app/Json.h"

namespace typeit::tui {
    namespace {

        /// One number and its name, in a fixed-width field so the block does
        /// not shift between a run of 9 WPM and one of 100.
        ftxui::Element figure(const std::string& label, const std::string& value, const Styling& name,
                              const Styling& number) {
            return ftxui::hbox({
                    ftxui::text("  " + label + std::string(label.size() < 18 ? 18 - label.size() : 1, ' ')) |
                            ftxui::color(name.color),
                    ftxui::text(fixed_width(value, 8)) | ftxui::color(number.color),
            });
        }

        std::string percent(core::Accuracy value) { return app::json::number(value.value * 100.0) + "%"; }

    }  // namespace

    ftxui::Element ResultsScreen::render() {
        const app::ColorDepth depth = context_->capabilities.color;
        const Styling accent = style_for(*context_->theme, app::ThemeColor::Accent, depth);
        const Styling muted = style_for(*context_->theme, app::ThemeColor::Muted, depth);
        const Styling plain = style_for(*context_->theme, app::ThemeColor::TextCorrect, depth);

        std::vector<ftxui::Element> rows;
        rows.push_back(ftxui::text(record_.completed ? "Run complete" : "Run abandoned") |
                       ftxui::color(record_.completed ? accent.color : muted.color));
        rows.push_back(ftxui::text(""));

        // Every number is formatted through the same helper the exports use, so
        // a figure on screen and the same figure in a CSV cannot disagree.
        rows.push_back(figure("net wpm", app::json::number(record_.net_wpm.value), muted, plain));
        rows.push_back(figure("gross wpm", app::json::number(record_.gross_wpm.value), muted, plain));
        rows.push_back(figure("raw wpm", app::json::number(record_.raw_wpm.value), muted, plain));
        rows.push_back(figure("accuracy", percent(record_.accuracy), muted, plain));
        rows.push_back(figure("correctness", percent(record_.final_correctness), muted, plain));
        rows.push_back(figure("consistency", app::json::number(record_.consistency), muted, plain));
        rows.push_back(figure("characters", std::to_string(record_.graphemes_typed), muted, plain));
        rows.push_back(figure("errors", std::to_string(record_.errors_total), muted, plain));
        rows.push_back(figure("seconds", app::json::number(static_cast<double>(record_.duration.value) / 1000.0), muted,
                              plain));

        rows.push_back(ftxui::text(""));
        const std::vector<Hint> hints{
                {.action = Action::Restart, .label = "again"},
                {.action = Action::NewText, .label = "new text"},
                {.action = Action::Menu, .label = "menu"},
        };
        rows.push_back(key_hint_bar(hints, *context_->keymap, *context_->theme, depth));

        return ftxui::vbox(std::move(rows));
    }

    bool ResultsScreen::on_event(ftxui::Event event) {
        const std::optional<Action> action = context_->keymap->action_for(event);
        if (!action.has_value()) {
            return false;
        }

        switch (*action) {
            case Action::Restart:
                requested_ = ResultsAction::Restart;
                return true;
            case Action::NewText:
                requested_ = ResultsAction::NewText;
                return true;
            case Action::QuitOrBack:
            case Action::Menu:
                requested_ = ResultsAction::Menu;
                return true;
            default:
                // Everything else — quit, history, the library — belongs to
                // whoever is driving the stack.
                return false;
        }
    }

    std::optional<ResultsAction> ResultsScreen::take_action() {
        std::optional<ResultsAction> taken = requested_;
        requested_.reset();
        return taken;
    }

}  // namespace typeit::tui
