// What to draw when there is nowhere to draw (TI-090).
//
// It is pushed *on top* of whatever was there, never in place of it, so a
// terminal shrunk mid-run and grown back finds the run exactly where it was.
// The session underneath is untouched — it is not even told this happened.
//
// The gate has hysteresis. A terminal sitting exactly on the boundary would
// otherwise flicker between two screens every time a redraw disagreed by one
// row, which is worse than either screen.
#ifndef TYPEIT_TUI_SCREENS_TERMINALTOOSMALLSCREEN_H
#define TYPEIT_TUI_SCREENS_TERMINALTOOSMALLSCREEN_H

#include <cstddef>
#include <ftxui/component/event.hpp>
#include <ftxui/dom/elements.hpp>
#include <string_view>

#include "IScreen.h"
#include "Layout.h"
#include "ScreenContext.h"

namespace typeit::tui {

    /// Rows and columns a terminal must gain past the minimum before the
    /// warning goes away again. Two of each: enough that a drag which wobbles
    /// by one does not toggle the screen, small enough that somebody
    /// deliberately resizing sees it clear.
    inline constexpr std::size_t kTooSmallHysteresis = 2;

    /// Whether the warning should be showing, remembering whether it already
    /// is. The one piece of state in the decision, and it is a bool with a
    /// reason rather than a navigation flag: it says what the terminal is, not
    /// where the program is.
    class TooSmallGate {
    public:
        /// Updates and returns whether the warning belongs on screen.
        [[nodiscard]] bool update(TerminalSize size);

        [[nodiscard]] bool showing() const noexcept { return showing_; }

    private:
        bool showing_ = false;
    };

    class TerminalTooSmallScreen : public IScreen {
    public:
        explicit TerminalTooSmallScreen(const ScreenContext& context) : context_{&context} {}

        [[nodiscard]] ftxui::Element render() override;

        /// Only quitting. Every other key is left alone: there is nothing here
        /// to navigate, and swallowing keys would leave somebody unable to
        /// leave a screen that appeared without being asked for.
        [[nodiscard]] bool on_event(ftxui::Event event) override;

        [[nodiscard]] std::string_view title() const override { return "terminal too small"; }

    private:
        const ScreenContext* context_;
    };

}  // namespace typeit::tui

#endif  // TYPEIT_TUI_SCREENS_TERMINALTOOSMALLSCREEN_H
