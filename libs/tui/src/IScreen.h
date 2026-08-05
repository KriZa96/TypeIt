// One screenful of the program, and the only thing that can be on top of the
// stack (TECHNICAL §3.2).
//
// Internal to `typeit::tui`, and it has to be: the interface names
// `ftxui::Element` and `ftxui::Event`, and FTXUI is linked `PRIVATE` so that
// nothing outside this library inherits it. The public surface of `tui` is
// `TerminalApp` and nothing else — the composition root asks for an
// application, not for a rendering vocabulary.
//
// Naming follows STYLE.md rather than TECHNICAL §3.2's sketch, which spells
// these `Render()` and `OnEvent()`. Those are FTXUI's names, correct on
// `TypingArea` because it genuinely overrides `ftxui::ComponentBase`; `IScreen`
// overrides nothing, so it is snake_case like every other method in the
// project.
#ifndef TYPEIT_TUI_ISCREEN_H
#define TYPEIT_TUI_ISCREEN_H

#include <ftxui/component/event.hpp>
#include <ftxui/dom/elements.hpp>
#include <string_view>

namespace typeit::tui {

    class IScreen {
    public:
        IScreen() = default;
        virtual ~IScreen() = default;
        IScreen(const IScreen&) = delete;
        IScreen& operator=(const IScreen&) = delete;
        IScreen(IScreen&&) = delete;
        IScreen& operator=(IScreen&&) = delete;

        /// What to draw. **Pure**: it reads state and returns elements, and
        /// mutates nothing — the one rule 1.0's rendering breaks, and the
        /// reason a redraw there can change what is being measured.
        [[nodiscard]] virtual ftxui::Element render() = 0;

        /// One event, already delivered to the top of the stack and nowhere
        /// else. Returns whether it was handled; `false` lets whatever is
        /// above the screen — the application's own quit binding — see it.
        [[nodiscard]] virtual bool on_event(ftxui::Event event) = 0;

        /// For the window title and for `--doctor`-style diagnostics. Stable:
        /// it identifies the screen, not what it currently shows.
        [[nodiscard]] virtual std::string_view title() const = 0;
    };

}  // namespace typeit::tui

#endif  // TYPEIT_TUI_ISCREEN_H
