#include "screens/TerminalTooSmallScreen.h"

#include <ftxui/component/event.hpp>
#include <ftxui/dom/elements.hpp>
#include <string>

#include "ColorQuantizer.h"
#include "Keymap.h"
#include "Layout.h"

namespace typeit::tui {

    bool TooSmallGate::update(TerminalSize size) {
        const bool below = size.columns < kMinimumSize.columns || size.rows < kMinimumSize.rows;
        if (!showing_) {
            showing_ = below;
            return showing_;
        }

        // Already showing: require room to spare before going away, so a drag
        // that wobbles by one row does not toggle the screen.
        const bool comfortably_above = size.columns >= kMinimumSize.columns + kTooSmallHysteresis &&
                                       size.rows >= kMinimumSize.rows + kTooSmallHysteresis;
        showing_ = !comfortably_above;
        return showing_;
    }

    ftxui::Element TerminalTooSmallScreen::render() {
        const Styling error = style_for(*context_->theme, app::ThemeColor::Error, context_->capabilities.color);
        const Styling muted = style_for(*context_->theme, app::ThemeColor::Muted, context_->capabilities.color);

        const std::string have = std::to_string(context_->size.columns) + "x" + std::to_string(context_->size.rows);
        const std::string need = std::to_string(kMinimumSize.columns) + "x" + std::to_string(kMinimumSize.rows);

        // Both numbers, because "too small" without them leaves somebody
        // resizing by guesswork.
        return ftxui::vbox({
                ftxui::text("The terminal is too small.") | ftxui::color(error.color),
                ftxui::text(""),
                ftxui::text("  now:  " + have) | ftxui::color(muted.color),
                ftxui::text("  need: " + need) | ftxui::color(muted.color),
                ftxui::text(""),
                ftxui::text("  Resize, or press " + to_string(context_->keymap->binding(Action::ForceQuit)) +
                            " to quit.") |
                        ftxui::color(muted.color),
        });
    }

    bool TerminalTooSmallScreen::on_event(ftxui::Event event) {
        // Quitting, and nothing else. Swallowing keys would leave somebody
        // unable to leave a screen that appeared without being asked for.
        return context_->keymap->action_for(event) == Action::ForceQuit;
    }

}  // namespace typeit::tui
