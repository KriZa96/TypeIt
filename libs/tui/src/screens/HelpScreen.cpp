#include "screens/HelpScreen.h"

#include <algorithm>
#include <cstddef>
#include <ftxui/component/event.hpp>
#include <ftxui/dom/elements.hpp>
#include <string>
#include <utility>
#include <vector>

#include "ColorQuantizer.h"
#include "Keymap.h"

namespace typeit::tui {
    namespace {

        /// What each action is called on screen. The enum is the vocabulary;
        /// this is the English.
        std::string describe(Action action) {
            switch (action) {
                case Action::QuitOrBack:
                    return "close this screen / go back";
                case Action::ForceQuit:
                    return "quit from anywhere";
                case Action::Restart:
                    return "restart the run with the same text";
                case Action::NewText:
                    return "a new text, same settings";
                case Action::Menu:
                    // No confirmation, and the text says so rather than
                    // promising one: leaving mid-run saves it as abandoned,
                    // which is not a destructive thing to need protecting from.
                    return "the menu (a run in progress is saved)";
                case Action::History:
                    return "history";
                case Action::TextLibrary:
                    return "text library";
                case Action::Settings:
                    return "settings";
                case Action::Help:
                    return "this screen";
            }
            return "";
        }

    }  // namespace

    ftxui::Element HelpScreen::render() {
        const Styling accent = style_for(*context_->theme, app::ThemeColor::Accent, context_->capabilities.color);
        const Styling muted = style_for(*context_->theme, app::ThemeColor::Muted, context_->capabilities.color);
        const Styling plain = style_for(*context_->theme, app::ThemeColor::TextCorrect, context_->capabilities.color);

        std::vector<ftxui::Element> all;
        all.push_back(ftxui::text("Keys") | ftxui::color(accent.color));
        all.push_back(ftxui::text(""));

        for (const Action action: kAllActions) {
            // Read from the keymap every time, so a rebind shows here without
            // anybody remembering to update a string.
            const std::string key = to_string(context_->keymap->binding(action));
            all.push_back(ftxui::hbox({
                    ftxui::text("  " + key + std::string(key.size() < 12 ? 12 - key.size() : 1, ' ')) |
                            ftxui::color(accent.color),
                    ftxui::text(describe(action)) | ftxui::color(plain.color),
            }));
        }

        all.push_back(ftxui::text(""));
        // UX §1: the one thing people ask for that a terminal program cannot
        // give them, said once here rather than in a bug report every month.
        all.push_back(ftxui::text("Font size is a setting of your terminal, not of TypeIt.") |
                      ftxui::color(muted.color));

        // Scrolled rather than truncated: a help screen that silently hides
        // half its keys is the problem it exists to solve.
        const std::size_t rows = std::max<std::size_t>(1, context_->size.rows);
        scrollable_ = all.size() > rows;
        const std::size_t first = std::min(scroll_, scrollable_ ? all.size() - rows : 0);

        std::vector<ftxui::Element> shown;
        for (std::size_t at = first; at < std::min(all.size(), first + rows); ++at) {
            shown.push_back(all.at(at));
        }
        return ftxui::vbox(std::move(shown));
    }

    bool HelpScreen::on_event(ftxui::Event event) {
        if (event == ftxui::Event::ArrowDown || event == ftxui::Event::PageDown) {
            ++scroll_;
            return true;
        }
        if (event == ftxui::Event::ArrowUp || event == ftxui::Event::PageUp) {
            scroll_ = scroll_ > 0 ? scroll_ - 1 : 0;
            return true;
        }
        // Leaving is the screen stack's, which pops on `QuitOrBack`.
        return false;
    }

}  // namespace typeit::tui
