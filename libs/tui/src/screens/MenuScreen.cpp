#include "screens/MenuScreen.h"

#include <algorithm>
#include <cstddef>
#include <ftxui/component/event.hpp>
#include <ftxui/dom/elements.hpp>
#include <optional>
#include <ranges>
#include <string>
#include <utility>
#include <vector>

#include "Bars.h"
#include "Charts.h"
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
                case MenuField::Text:
                    return "text";
                case MenuField::Start:
                    return "start";
            }
            return "";
        }

        /// Ten, which is what UX §3.1 shows and what fits beside the figures.
        constexpr std::size_t kRecentRuns = 10;

        std::string whole(double value) { return std::to_string(static_cast<std::int64_t>(value)); }

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
        load_recent();
    }

    void MenuScreen::load_recent() {
        // Read once, here. A query in `render` would run against a file on disk
        // every frame, and the last ten runs do not change while somebody
        // chooses a duration.
        if (context_->history.records == nullptr) {
            return;
        }
        app::HistoryFilter filter;
        filter.limit = kRecentRuns;
        const core::Result<std::vector<app::SessionRow>> rows = context_->history.records->query(filter);
        if (!rows) {
            // The menu still works without it. A run nobody can start because
            // the sparkline failed to load would be the worse trade.
            return;
        }

        double total = 0.0;
        double accuracy = 0.0;
        // Oldest first: `query` gives newest first, and a trend line that runs
        // backwards is a trend line pointing the wrong way.
        for (const app::SessionRow& row: std::ranges::reverse_view(*rows)) {
            recent_.wpm.push_back(row.net_wpm.value);
            total += row.net_wpm.value;
            accuracy += row.accuracy.value;
            recent_.best = core::Wpm{std::max(recent_.best.value, row.net_wpm.value)};
        }
        if (!recent_.wpm.empty()) {
            const auto count = static_cast<double>(recent_.wpm.size());
            recent_.mean = core::Wpm{total / count};
            recent_.accuracy = core::Accuracy{accuracy / count};
        }
    }

    std::size_t MenuScreen::text_choices() const { return context_->texts == nullptr ? 0 : context_->texts->size(); }

    bool MenuScreen::typing_a_path() const {
        // With nothing to choose between there is no "custom" position either:
        // the run types whatever the composition root supplied, and a path
        // field nobody can act on is worse than no field at all.
        return text_choices() != 0 && selection_.text >= text_choices();
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
        if (focused_ == MenuField::Text && typing_a_path()) {
            if (!selection_.custom_path.empty()) {
                selection_.custom_path.pop_back();
            }
            static_cast<void>(validate());
            return;
        }
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
        if (typing_a_path()) {
            if (selection_.custom_path.empty()) {
                message_ = "type the path of a text file";
                return false;
            }
            // Actually opened, not merely checked for existence: "it is there"
            // and "I can read it" are different answers, and the second is the
            // one the run needs. The corpora are a kilobyte each, so reading
            // one per keystroke costs nothing worth avoiding.
            if (const core::Result<std::string> text = context_->load_text(selection_.custom_path); !text) {
                message_ = core::to_string(text.error());
                return false;
            }
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
                case MenuField::Text:
                    if (text_choices() == 0) {
                        value = "built-in";
                    } else if (typing_a_path()) {
                        value = "custom  " + selection_.custom_path;
                    } else {
                        value = context_->texts->at(selection_.text).name;
                    }
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

        // The point of recording history is to see it without asking, so it is
        // on the menu rather than behind a key.
        rows.push_back(ftxui::text(""));
        if (recent_.wpm.empty()) {
            rows.push_back(ftxui::text("  no runs yet") | ftxui::color(muted.color));
        } else {
            rows.push_back(ftxui::hbox({
                    ftxui::text("  last " + std::to_string(recent_.wpm.size()) + "  ") | ftxui::color(muted.color),
                    sparkline(recent_.wpm, *context_->theme, {.width = kRecentRuns, .glyphs = glyphs, .depth = depth}),
                    ftxui::text("   avg ") | ftxui::color(muted.color),
                    ftxui::text(whole(recent_.mean.value)) | ftxui::color(accent.color),
                    ftxui::text(" best ") | ftxui::color(muted.color),
                    ftxui::text(whole(recent_.best.value)) | ftxui::color(accent.color),
                    ftxui::text(" acc ") | ftxui::color(muted.color),
                    ftxui::text(whole(recent_.accuracy.value * 100.0) + "%") | ftxui::color(accent.color),
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

    void MenuScreen::request_start() {
        // Refused with a message rather than silently doing nothing, which is
        // what an invalid selection does in 1.0.
        start_requested_ = validate();
        if (!start_requested_ && message_.empty()) {
            message_ = "that selection cannot be started";
        }
    }

    bool MenuScreen::typed(char letter) {
        if (focused_ == MenuField::Text && typing_a_path()) {
            // A path takes every printable character, digits included, so this
            // comes before the numeric field.
            selection_.custom_path += letter;
            static_cast<void>(validate());
            return true;
        }
        if (letter >= '0' && letter <= '9') {
            edit(letter);
            return true;
        }
        // Left and right cycle the lists; a letter is not how one is chosen,
        // and swallowing letters here would eat the help key.
        return false;
    }

    bool MenuScreen::cycle(bool forward) {
        if (focused_ == MenuField::Text && text_choices() != 0) {
            // One position past the last entry is "a path I type myself",
            // which is where 1.0's fourth radio button sat too.
            const std::size_t count = text_choices() + 1;
            selection_.text = forward ? (selection_.text + 1) % count : (selection_.text + count - 1) % count;
            static_cast<void>(validate());
            return true;
        }

        if (focused_ == MenuField::Mode) {
            // NOLINTNEXTLINE(readability-qualified-auto) -- MSVC's array iterator is not a pointer
            const auto at = std::ranges::find(core::kModeNames, selection_.mode);
            const std::size_t index =
                    at == core::kModeNames.end() ? 0 : static_cast<std::size_t>(at - core::kModeNames.begin());
            const std::size_t count = core::kModeNames.size();
            selection_.mode = core::kModeNames.at(forward ? (index + 1) % count : (index + count - 1) % count);
            static_cast<void>(validate());
            return true;
        }

        return false;
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
            request_start();
            return true;
        }
        if (event == ftxui::Event::Backspace) {
            backspace_field();
            return true;
        }
        if (event.is_character() && event.character().size() == 1) {
            return typed(event.character().front());
        }
        if (event == ftxui::Event::ArrowLeft || event == ftxui::Event::ArrowRight) {
            return cycle(event == ftxui::Event::ArrowRight);
        }
        return false;
    }

    bool MenuScreen::take_start() {
        const bool asked = start_requested_;
        start_requested_ = false;
        return asked;
    }

}  // namespace typeit::tui
