#include "typeit/tui/TerminalApp.h"

#include <ftxui/component/component.hpp>
#include <ftxui/component/screen_interactive.hpp>
#include <ftxui/dom/elements.hpp>
#include <memory>
#include <utility>

namespace typeit::tui {

    /// The whole of FTXUI, kept here. The header opposite names none of it, so
    /// nothing that links this library has to know it exists.
    struct TerminalApp::Impl {
        /// Fullscreen: a typing test that scrolls the shell's history away as
        /// it redraws is unusable, and the alternate buffer is what a terminal
        /// application is for.
        ftxui::ScreenInteractive screen = ftxui::ScreenInteractive::Fullscreen();
        bool quitting = false;
    };

    TerminalApp::TerminalApp() : impl_{std::make_unique<Impl>()} {}

    TerminalApp::~TerminalApp() = default;

    void TerminalApp::run() {
        if (impl_->quitting) {
            // "Stop" arrived before "start". Entering the loop to leave it
            // again would still take the terminal over and hand it back, which
            // is a visible flicker for no reason.
            return;
        }

        // Nothing to draw yet: screens are TI-081 and the stack they live on is
        // what will render here. An empty frame is the honest placeholder — it
        // proves the loop runs and exits, and it is one line to replace.
        const ftxui::Component blank = ftxui::Renderer([] { return ftxui::text(""); });
        impl_->screen.Loop(blank);
    }

    void TerminalApp::quit() {
        if (std::exchange(impl_->quitting, true)) {
            return;
        }
        // `ExitLoopClosure` is safe to call when no loop is running: it posts a
        // task the loop reads on its next turn, and a loop that never starts
        // simply never reads it.
        impl_->screen.ExitLoopClosure()();
    }

    bool TerminalApp::is_quitting() const noexcept { return impl_->quitting; }

}  // namespace typeit::tui
