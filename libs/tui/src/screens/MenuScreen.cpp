#include "screens/MenuScreen.h"

#include <algorithm>
#include <cstddef>
#include <ftxui/component/event.hpp>
#include <ftxui/dom/elements.hpp>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "Bars.h"
#include "ColorQuantizer.h"
#include "Glyphs.h"
#include "Keymap.h"
#include "typeit/core/config/Validation.h"

namespace typeit::tui {
    namespace {

        std::string label_for(MenuField field) {
            switch (field) {
                case MenuField::Mode:
                    return "mode";
                case MenuField::Duration:
                    return "seconds";
                case MenuField::WordCount:
                    return "words";
                case MenuField::Start:
                    return "start";
            }
            return "";
        }

        std::string range_message(std::string_view field, core::Range range) {
            return std::string{field} + " must be between " + std::to_string(range.low) + " and " +
                   std::to_string(range.high);
        }

    }  // namespace

    MenuScreen::MenuScreen(const ScreenContext& context) : context_{&context} {
        // The configuration is where a menu starts, so somebody who set a
        // default duration does not have to set it again every time.
        selection_.mode = context.config->general.default_mode;
        selection_.seconds = context.config->general.default_duration_s;
        selection_.words = context.config->general.default_word_count;
    }

    void MenuScreen::focus_next() {
        const auto at = static_cast<std::size_t>(focused_);
        focused_ = kMenuFields.at((at + 1) % kMenuFields.size());
        editing_.clear();
    }

    void MenuScreen::focus_previous() {
        const auto at = static_cast<std::size_t>(focused_);
        focused_ = kMenuFields.at((at + kMenuFields.size() - 1) % kMenuFields.size());
        editing_.clear();
    }

    void MenuScreen::edit(char digit) {
        if (focused_ != MenuField::Duration && focused_ != MenuField::WordCount) {
            return;
        }
        // Capped so a long run of digits cannot overflow the parse. The range
        // check below rejects anything silly anyway; this stops it being
        // undefined first.
        if (editing_.size() < 6) {
            editing_ += digit;
        }

        const std::int64_t value = editing_.empty() ? 0 : std::stoll(editing_);
        if (focused_ == MenuField::Duration) {
            selection_.seconds = value;
        } else {
            selection_.words = value;
        }
        // **Now**, not while rendering. 1.0 parses inside a render callback
        // with an empty catch block, so a bad value is swallowed sixty times a
        // second and nobody is told.
        static_cast<void>(validate());
    }

    void MenuScreen::backspace_field() {
        if (editing_.empty()) {
            return;
        }
        editing_.pop_back();

        const std::int64_t value = editing_.empty() ? 0 : std::stoll(editing_);
        if (focused_ == MenuField::Duration) {
            selection_.seconds = value;
        } else if (focused_ == MenuField::WordCount) {
            selection_.words = value;
        }
        static_cast<void>(validate());
    }

    bool MenuScreen::validate() {
        // The same ranges the configuration file enforces. A menu that accepted
        // what the file refuses would be a second, quieter set of rules.
        if (selection_.seconds < core::kDurationSeconds.low || selection_.seconds > core::kDurationSeconds.high) {
            message_ = range_message("seconds", core::kDurationSeconds);
            return false;
        }
        if (selection_.words < core::kWordCount.low || selection_.words > core::kWordCount.high) {
            message_ = range_message("words", core::kWordCount);
            return false;
        }
        if (std::ranges::find(core::kModeNames, selection_.mode) == core::kModeNames.end()) {
            message_ = "no mode called \"" + selection_.mode + "\"";
            return false;
        }

        message_.clear();
        return true;
    }

    ftxui::Element MenuScreen::render() {
        const app::ColorDepth depth = context_->capabilities.color;
        const Styling accent = style_for(*context_->theme, app::ThemeColor::Accent, depth);
        const Styling muted = style_for(*context_->theme, app::ThemeColor::Muted, depth);
        const Styling error = style_for(*context_->theme, app::ThemeColor::Error, depth);
        const app::GlyphSet glyphs = context_->capabilities.glyphs;

        std::vector<ftxui::Element> rows;
        rows.push_back(ftxui::text("TypeIt") | ftxui::color(accent.color));
        rows.push_back(ftxui::text(""));

        for (const MenuField field: kMenuFields) {
            const bool here = field == focused_;
            const std::string marker{glyph(glyphs, here ? Glyph::RadioSelected : Glyph::RadioEmpty)};

            std::string value;
            switch (field) {
                case MenuField::Mode:
                    value = selection_.mode;
                    break;
                case MenuField::Duration:
                    value = std::to_string(selection_.seconds);
                    break;
                case MenuField::WordCount:
                    value = std::to_string(selection_.words);
                    break;
                case MenuField::Start:
                    break;
            }

            const std::string name = label_for(field);
            rows.push_back(ftxui::hbox({
                    ftxui::text("  " + marker + " ") | ftxui::color(here ? accent.color : muted.color),
                    ftxui::text(name + std::string(name.size() < 10 ? 10 - name.size() : 1, ' ')) |
                            ftxui::color(muted.color),
                    ftxui::text(value) | ftxui::color(accent.color),
            }));
        }

        if (!message_.empty()) {
            // Visible, and it stays visible until the value is fixed — the
            // whole difference from a silently swallowed exception.
            rows.push_back(ftxui::text(""));
            rows.push_back(ftxui::text("  " + message_) | ftxui::color(error.color));
        }

        rows.push_back(ftxui::text(""));
        rows.push_back(key_hint_bar(
                {{.action = Action::Help, .label = "keys"}, {.action = Action::ForceQuit, .label = "quit"}},
                *context_->keymap, *context_->theme, depth));
        return ftxui::vbox(std::move(rows));
    }

    bool MenuScreen::on_event(ftxui::Event event) {
        if (event == ftxui::Event::Tab || event == ftxui::Event::ArrowDown) {
            focus_next();
            return true;
        }
        if (event == ftxui::Event::TabReverse || event == ftxui::Event::ArrowUp) {
            focus_previous();
            return true;
        }

        if (event == ftxui::Event::Return) {
            // Refused with a message rather than silently doing nothing, which
            // is what an invalid selection does in 1.0.
            start_requested_ = validate();
            if (!start_requested_ && message_.empty()) {
                message_ = "that selection cannot be started";
            }
            return true;
        }

        if (event == ftxui::Event::Backspace) {
            backspace_field();
            return true;
        }

        if (event.is_character() && event.character().size() == 1) {
            const char typed = event.character().front();
            if (typed >= '0' && typed <= '9') {
                edit(typed);
                return true;
            }
            if (focused_ == MenuField::Mode) {
                // Left and right cycle the mode; a letter is not how a mode is
                // chosen, and swallowing letters here would eat the help key.
                return false;
            }
        }

        if (focused_ == MenuField::Mode && (event == ftxui::Event::ArrowLeft || event == ftxui::Event::ArrowRight)) {
            // NOLINTNEXTLINE(readability-qualified-auto) -- MSVC's array iterator is not a pointer
            const auto at = std::ranges::find(core::kModeNames, selection_.mode);
            const std::size_t index =
                    at == core::kModeNames.end() ? 0 : static_cast<std::size_t>(at - core::kModeNames.begin());
            const std::size_t count = core::kModeNames.size();
            const std::size_t next =
                    event == ftxui::Event::ArrowRight ? (index + 1) % count : (index + count - 1) % count;
            selection_.mode = core::kModeNames.at(next);
            static_cast<void>(validate());
            return true;
        }

        return false;
    }

    bool MenuScreen::take_start() {
        const bool asked = start_requested_;
        start_requested_ = false;
        return asked;
    }

}  // namespace typeit::tui
