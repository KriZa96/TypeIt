// The keys, as they are actually bound (TI-094).
//
// Read from the keymap on every render, never from a list somebody keeps in
// step by hand — a help screen that shows the defaults after a rebind is worse
// than no help screen, because it is confidently wrong.
#ifndef TYPEIT_TUI_SCREENS_HELPSCREEN_H
#define TYPEIT_TUI_SCREENS_HELPSCREEN_H

#include <cstddef>
#include <ftxui/component/event.hpp>
#include <ftxui/dom/elements.hpp>
#include <string_view>

#include "IScreen.h"
#include "ScreenContext.h"

namespace typeit::tui {

    class HelpScreen : public IScreen {
    public:
        explicit HelpScreen(const ScreenContext& context) : context_{&context} {}

        [[nodiscard]] ftxui::Element render() override;

        /// Scrolling and leaving. Everything else is somebody else's.
        [[nodiscard]] bool on_event(ftxui::Event event) override;

        [[nodiscard]] std::string_view title() const override { return "help"; }

        /// The first line shown, so a test can assert scrolling rather than
        /// infer it from pixels.
        [[nodiscard]] std::size_t scroll() const noexcept { return scroll_; }

        /// Whether the last render had more to show than fitted.
        [[nodiscard]] bool scrollable() const noexcept { return scrollable_; }

    private:
        const ScreenContext* context_;
        std::size_t scroll_ = 0;
        bool scrollable_ = false;
    };

}  // namespace typeit::tui

#endif  // TYPEIT_TUI_SCREENS_HELPSCREEN_H
