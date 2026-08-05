// The centrepiece (TI-087, ADR-011, TECHNICAL §3.1).
//
// A component that consumes key events itself. **No `ftxui::Input`, no bound
// `std::string`.** 1.0 binds an Input to a string and then infers "the
// character just typed" from `input_text_.back()` — which is one byte, not one
// grapheme, and is the actual source of the ćčšđž problem the README blames on
// FTXUI. `Event::Character` already carries a complete UTF-8 sequence; taking
// it directly is both simpler and correct.
//
// It owns no text. It renders from the session's model and mutates through the
// session, so the mode hears about every keystroke exactly as it does in a
// scripted run — one code path, not two.
//
// **`render()` mutates nothing.** That is the rule 1.0 breaks, and the reason
// a redraw there can change what is being measured. There is a test that
// renders twice and compares the model byte for byte.
#ifndef TYPEIT_TUI_TYPINGAREA_H
#define TYPEIT_TUI_TYPINGAREA_H

#include <cstddef>
#include <ftxui/component/component_base.hpp>
#include <ftxui/component/event.hpp>
#include <ftxui/dom/elements.hpp>

#include "typeit/app/Capabilities.h"
#include "typeit/app/Theme.h"
#include "typeit/core/session/Session.h"
#include "typeit/core/text/Wrapper.h"
#include "typeit/core/util/IClock.h"

namespace typeit::tui {

    struct TypingAreaOptions {
        /// Columns to wrap at. Zero means "fit the terminal", which is the
        /// setting most people want and the one a naive range check rejects.
        std::size_t columns = 0;
        /// Lines of text visible around the cursor.
        std::size_t lines_visible = 3;
        app::ColorDepth depth = app::ColorDepth::TrueColor;
        app::GlyphSet glyphs = app::GlyphSet::Unicode;
        /// Hide correctness colouring until the run ends. Nothing about what
        /// was typed changes — only whether the typist can see it.
        bool blind = false;
    };

    class TypingArea : public ftxui::ComponentBase {
    public:
        /// The session and the theme must outlive the component; the screen
        /// owns it and the application owns them.
        ///
        /// The clock is how a keystroke gets its timestamp. It is a parameter
        /// rather than a call to `now()` inside the widget for the same reason
        /// it is everywhere else: a scripted run has to be able to supply its
        /// own.
        TypingArea(core::Session& session, const app::Theme& theme, const core::IClock& clock,
                   TypingAreaOptions options = {});

        /// Draws the text, per-grapheme state colouring, and the caret.
        ///
        /// Pure. It re-wraps when the column count has changed, which is the
        /// whole resize story, and that is a cache — the same input gives the
        /// same output.
        ftxui::Element Render() override;

        /// **The only place the model mutates.** `Event::Character` carries a
        /// complete UTF-8 sequence; `Event::Backspace` deletes. Everything else
        /// returns false so the screen above can handle navigation.
        bool OnEvent(ftxui::Event event) override;

        bool Focusable() const override { return true; }

        /// The columns last rendered at, so a test can assert a re-wrap
        /// happened rather than infer it.
        [[nodiscard]] std::size_t columns() const noexcept { return last_columns_; }

    private:
        [[nodiscard]] std::size_t columns_for(std::size_t available) const;
        /// One wrapped line, by its index in `breaks_`. The index rather than
        /// a pair of offsets: the range is the wrapper's to say, and two
        /// adjacent `size_t` parameters are two that can be swapped.
        [[nodiscard]] ftxui::Element render_line(std::size_t line) const;
        void rewrap(std::size_t columns);

        core::Session* session_;
        const app::Theme* theme_;
        const core::IClock* clock_;
        TypingAreaOptions options_;

        core::LineBreaks breaks_;
        std::size_t last_columns_ = 0;
    };

}  // namespace typeit::tui

#endif  // TYPEIT_TUI_TYPINGAREA_H
