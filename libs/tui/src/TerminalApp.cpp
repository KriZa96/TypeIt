#include "typeit/tui/TerminalApp.h"

#include <ftxui/component/component.hpp>
#include <ftxui/component/event.hpp>
#include <ftxui/component/screen_interactive.hpp>
#include <ftxui/dom/elements.hpp>
#include <ftxui/screen/terminal.hpp>
#include <memory>
#include <optional>
#include <utility>

#include "Keymap.h"
#include "ScreenContext.h"
#include "ScreenStack.h"
#include "WindowsConsole.h"
#include "screens/HelpScreen.h"
#include "screens/MenuScreen.h"
#include "screens/ResultsScreen.h"
#include "screens/SessionScreen.h"
#include "screens/TerminalTooSmallScreen.h"
#include "typeit/tui/FrameTicker.h"

namespace typeit::tui {

    /// The whole of FTXUI and the whole of the navigation, kept here. The
    /// header opposite names none of it.
    struct TerminalApp::Impl {
        /// Fullscreen: a typing test that scrolls the shell's history away as
        /// it redraws is unusable, and the alternate buffer is what a terminal
        /// application is for.
        /// Configured and, more importantly, put back afterwards. Declared
        /// first so it is destroyed last: the console must still be ours while
        /// FTXUI is tearing its screen down. Off Windows it does nothing.
        WindowsConsole console;

        ftxui::ScreenInteractive screen = ftxui::ScreenInteractive::Fullscreen();
        Dependencies dependencies;
        Keymap keymap;
        ScreenContext context;
        ScreenStack stack;
        TooSmallGate gate;
        /// The screen the run is on, borrowed so the tick can reach it. Null
        /// whenever the top is not a session.
        SessionScreen* session = nullptr;
        bool quitting = false;

        explicit Impl(Dependencies given) :
            // `dependencies` is declared first, so it is constructed first and
            // the keymap reads from it rather than from the moved-out argument.
            dependencies{std::move(given)}, keymap{Keymap::from_config(dependencies.config->keys)} {
            context.theme = dependencies.theme;
            context.keymap = &keymap;
            context.config = dependencies.config;
            context.capabilities = dependencies.capabilities;
            context.size = TerminalSize{.columns = 80, .rows = 24};
        }

        /// The size FTXUI last measured. Read every frame rather than cached,
        /// because that is the whole of the resize story.
        void measure() {
            const ftxui::Dimensions size = ftxui::Terminal::Size();
            context.size = TerminalSize{.columns = static_cast<std::size_t>(size.dimx),
                                        .rows = static_cast<std::size_t>(size.dimy)};
        }

        /// Starts a run from what the menu chose.
        void start(const MenuSelection& selection) {
            app::SessionRequest request;
            request.mode = selection.mode;
            // The parameter as JSON, which is what the record carries. The mode
            // itself is resolved by the registry the composition root built.
            request.mode_param = selection.mode == "words" ? R"({"words":)" + std::to_string(selection.words) + "}"
                                                           : R"({"seconds":)" + std::to_string(selection.seconds) + "}";
            request.text = dependencies.text;
            request.rules = dependencies.config->typing;

            core::Result<std::unique_ptr<SessionScreen>> started =
                    SessionScreen::create(context, *dependencies.sessions, request, *dependencies.clock);
            if (!started) {
                // Nothing to do but stay where we are. The menu is still there
                // and still says what is wrong.
                return;
            }
            session = started->get();
            stack.push(std::shared_ptr<IScreen>{std::move(*started)});
        }

        /// Starts another run of whatever the menu underneath still holds.
        ///
        /// The menu is the only record of what was chosen: reading a default
        /// `MenuSelection` instead would silently restart a sixty-second run as
        /// a thirty-second one.
        void start_again() {
            if (auto* const menu = dynamic_cast<MenuScreen*>(&stack.top()); menu != nullptr) {
                start(menu->selection());
            }
        }

        /// Whatever the top screen has asked for since the last frame.
        void apply_requests() {
            if (stack.empty()) {
                return;
            }

            if (auto* const menu = dynamic_cast<MenuScreen*>(&stack.top()); menu != nullptr) {
                if (menu->take_start()) {
                    start(menu->selection());
                }
                return;
            }

            if (auto* const run = dynamic_cast<SessionScreen*>(&stack.top()); run != nullptr) {
                if (const std::optional<SessionOutcome> outcome = run->take_outcome(); outcome.has_value()) {
                    const app::SessionRecord record =
                            run->result().has_value() ? run->result()->record : app::SessionRecord{};
                    const bool again = *outcome == SessionOutcome::Restart;
                    session = nullptr;
                    stack.pop();
                    if (again) {
                        start_again();
                    } else if (*outcome == SessionOutcome::Finished) {
                        stack.push(std::make_shared<ResultsScreen>(context, record));
                    }
                }
                return;
            }

            if (auto* const results = dynamic_cast<ResultsScreen*>(&stack.top()); results != nullptr) {
                if (const std::optional<ResultsAction> action = results->take_action(); action.has_value()) {
                    stack.pop();
                    if (*action != ResultsAction::Menu) {
                        // Restart and "new text" both start another run; the
                        // library that would make them differ is Phase 6.
                        start_again();
                    }
                }
            }
        }
    };

    TerminalApp::TerminalApp(Dependencies dependencies) : impl_{std::make_unique<Impl>(std::move(dependencies))} {
        impl_->stack.push(std::make_shared<MenuScreen>(impl_->context));
    }

    TerminalApp::~TerminalApp() = default;

    void TerminalApp::run() {
        if (impl_->quitting) {
            // "Stop" arrived before "start". Entering the loop to leave it
            // again would still take the terminal over and hand it back, which
            // is a visible flicker for no reason.
            return;
        }

        // One thread, posting. It never touches the model — that is ADR-012 and
        // it is what makes the domain single-threaded by contract.
        const FrameTicker ticker{[this] { impl_->screen.PostEvent(ftxui::Event::Custom); }};

        const ftxui::Component ui = ftxui::Renderer([this] {
            impl_->measure();
            if (impl_->gate.update(impl_->context.size)) {
                // Pushed over whatever was there, never in place of it, so the
                // run underneath survives being made invisible.
                if (impl_->stack.top().title() != "terminal too small") {
                    impl_->stack.push(std::make_shared<TerminalTooSmallScreen>(impl_->context));
                }
            } else if (impl_->stack.top().title() == "terminal too small") {
                impl_->stack.pop();
            }
            return impl_->stack.render();
        });

        const ftxui::Component with_events = ftxui::CatchEvent(ui, [this](const ftxui::Event& event) {
            if (event == ftxui::Event::Custom) {
                // A frame. Time passes here and nowhere else.
                if (impl_->session != nullptr) {
                    impl_->session->on_tick(impl_->dependencies.clock->now());
                }
                impl_->apply_requests();
                return true;
            }

            if (impl_->keymap.action_for(event) == Action::ForceQuit) {
                quit();
                return true;
            }
            if (impl_->keymap.action_for(event) == Action::Help && impl_->stack.top().title() != "help") {
                impl_->stack.push(std::make_shared<HelpScreen>(impl_->context));
                return true;
            }

            const bool handled = impl_->stack.on_event(event);
            impl_->apply_requests();

            if (!handled && impl_->keymap.action_for(event) == Action::QuitOrBack) {
                // Nothing wanted it, so it means "go back". At the root that is
                // refused, which is why leaving is a separate binding.
                if (impl_->stack.size() > 1) {
                    impl_->stack.pop();
                } else {
                    quit();
                }
            }
            return true;
        });

        impl_->screen.Loop(with_events);
    }

    void TerminalApp::quit() {
        if (std::exchange(impl_->quitting, true)) {
            return;
        }
        // A run still in progress is saved as abandoned rather than dropped: it
        // happened, and quitting is not a reason to pretend otherwise.
        if (impl_->session != nullptr) {
            impl_->session->abandon();
            impl_->session = nullptr;
        }
        // `ExitLoopClosure` is safe to call when no loop is running: it posts a
        // task the loop reads on its next turn, and a loop that never starts
        // simply never reads it.
        impl_->screen.ExitLoopClosure()();
    }

    bool TerminalApp::is_quitting() const noexcept { return impl_->quitting; }

}  // namespace typeit::tui
