#include "TypingArea.h"

#include <algorithm>
#include <cstddef>
#include <ftxui/component/event.hpp>
#include <ftxui/dom/elements.hpp>
#include <span>
#include <string>
#include <utility>
#include <vector>

#include "ColorQuantizer.h"
#include "Glyphs.h"
#include "typeit/app/Theme.h"
#include "typeit/core/session/Session.h"
#include "typeit/core/session/TypingModel.h"
#include "typeit/core/text/Grapheme.h"
#include "typeit/core/text/TextBuffer.h"
#include "typeit/core/text/Whitespace.h"
#include "typeit/core/text/Wrapper.h"

namespace typeit::tui {
    namespace {

        /// Which theme colour a position is drawn in.
        app::ThemeColor color_for(core::GraphemeState state) {
            switch (state) {
                case core::GraphemeState::Correct:
                    return app::ThemeColor::TextCorrect;
                case core::GraphemeState::Incorrect:
                    return app::ThemeColor::TextIncorrect;
                case core::GraphemeState::Corrected:
                    return app::ThemeColor::TextCorrected;
                case core::GraphemeState::Missed:
                    // Skipped over. It is wrong in the finished text, and
                    // drawing it as pending would hide that.
                    return app::ThemeColor::TextIncorrect;
                case core::GraphemeState::Pending:
                    return app::ThemeColor::TextPending;
            }
            return app::ThemeColor::TextPending;
        }

        ftxui::Element styled(const std::string& text, const Styling& styling) {
            ftxui::Element element = ftxui::text(text) | ftxui::color(styling.color);
            if (styling.bold) {
                element = element | ftxui::bold;
            }
            if (styling.underline) {
                element = element | ftxui::underlined;
            }
            if (styling.inverted) {
                element = element | ftxui::inverted;
            }
            return element;
        }

    }  // namespace

    TypingArea::TypingArea(core::Session& session, const app::Theme& theme, const core::IClock& clock,
                           TypingAreaOptions options) :
        session_{&session}, theme_{&theme}, clock_{&clock}, options_{options} {}

    std::size_t TypingArea::columns_for(std::size_t available) const {
        // Zero means "fit the terminal", which is the setting most people want.
        // One column is the floor: `wrap` requires it, and a zero-width line
        // would never advance.
        const std::size_t wanted = options_.columns == 0 ? available : options_.columns;
        return std::max<std::size_t>(1, std::min(wanted, std::max<std::size_t>(1, available)));
    }

    void TypingArea::rewrap(std::size_t columns) {
        breaks_ = core::wrap(session_->text().graphemes(), columns);
        last_columns_ = columns;
    }

    ftxui::Element TypingArea::render_line(std::size_t line) const {
        const core::TypingModel& model = session_->model();
        const std::size_t from = breaks_.starts.at(line).value;
        const std::size_t to = line + 1 < breaks_.size() ? breaks_.starts.at(line + 1).value : model.target().size();
        const std::span<const core::Grapheme> target = model.target();
        const std::span<const core::GraphemeState> states = model.states();
        const std::size_t cursor = model.cursor().value;

        std::vector<ftxui::Element> cells;
        for (std::size_t at = from; at < to; ++at) {
            // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-avoid-unchecked-container-access) -- the wrapper's bounds
            const core::Grapheme& grapheme = target[at];
            // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-avoid-unchecked-container-access) -- one state per position
            const core::GraphemeState state = states[at];

            // A space typed wrongly is drawn as an underscore. Nothing else
            // shows a mistake on a character with no ink of its own, and 1.0
            // got this one right.
            const bool wrong = state == core::GraphemeState::Incorrect || state == core::GraphemeState::Missed;
            std::string ink{grapheme.view()};
            if (wrong && core::is_word_separator(grapheme)) {
                ink = "_";
            } else if (ink == "\n" || ink == "\r\n") {
                // A newline is a break, not a glyph: the wrapper has already
                // put what follows on the next line.
                ink = " ";
            }

            const app::ThemeColor which = options_.blind ? app::ThemeColor::TextPending : color_for(state);
            Styling styling = style_for(*theme_, which, options_.depth);
            if (at == cursor) {
                // The caret is where the next keystroke lands, whatever state
                // the position is in.
                styling = style_for(*theme_, app::ThemeColor::Caret, options_.depth);
                styling.inverted = true;
            }
            cells.push_back(styled(ink, styling));
        }

        if (cells.empty()) {
            return ftxui::text("");
        }
        return ftxui::hbox(std::move(cells));
    }

    ftxui::Element TypingArea::Render() {
        const core::TypingModel& model = session_->model();

        // FTXUI does not tell a component its width before it draws, so the
        // configured width is used when there is one and a sensible default
        // otherwise. `Layout` supplies the real terminal size (TI-089).
        const std::size_t columns = columns_for(options_.columns == 0 ? 80 : options_.columns);
        if (columns != last_columns_ || breaks_.empty()) {
            // A cache, not a mutation: the same text at the same width gives
            // the same breaks, so rendering twice still renders the same.
            rewrap(columns);
        }

        // Which line the cursor is on, so a long text scrolls with it rather
        // than drawing from the top forever.
        const std::size_t cursor = model.cursor().value;
        std::size_t cursor_line = 0;
        for (std::size_t line = 0; line < breaks_.size(); ++line) {
            if (breaks_.starts.at(line).value <= cursor) {
                cursor_line = line;
            }
        }

        const std::size_t first_line =
                cursor_line >= options_.lines_visible ? cursor_line - options_.lines_visible + 1 : 0;
        const std::size_t last_line = std::min(breaks_.size(), first_line + options_.lines_visible);

        std::vector<ftxui::Element> lines;
        for (std::size_t line = first_line; line < last_line; ++line) {
            lines.push_back(render_line(line));
        }

        if (lines.empty()) {
            lines.push_back(ftxui::text(""));
        }
        return ftxui::vbox(std::move(lines));
    }

    bool TypingArea::OnEvent(ftxui::Event event) {
        if (event == ftxui::Event::Backspace) {
            session_->on_backspace(clock_->now());
            return true;
        }

        if (!event.is_character()) {
            // Navigation, resize, anything else: the screen above handles it.
            return false;
        }

        // `Event::Character` already carries a complete UTF-8 sequence, which
        // is the whole of ADR-011: 1.0 reads back the last *byte* of a bound
        // string and calls it the character typed.
        //
        // A paste arrives as one event with several graphemes in it. Each is
        // typed in turn at the same instant rather than dropped — a paste is
        // something people do, and losing all but the first would desynchronise
        // the model from what the screen shows.
        const core::Result<core::TextBuffer> typed = core::TextBuffer::from_utf8(event.character());
        if (!typed) {
            // Not text. Nothing to type, and nothing to say about it here.
            return false;
        }

        const core::Millis at = clock_->now();
        for (const core::Grapheme& grapheme: typed->graphemes()) {
            session_->on_key(grapheme, at);
        }
        return true;
    }

}  // namespace typeit::tui
