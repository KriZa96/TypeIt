#include "typeit/tui/TerminalApp.h"

#include <cstdint>
#include <filesystem>
#include <ftxui/component/component.hpp>
#include <ftxui/component/event.hpp>
#include <ftxui/component/screen_interactive.hpp>
#include <ftxui/dom/elements.hpp>
#include <ftxui/screen/terminal.hpp>
#include <memory>
#include <optional>
#include <string>
#include <utility>

#include "ConsoleMode.h"
#include "Keymap.h"
#include "ScreenContext.h"
#include "ScreenStack.h"
#include "screens/HelpScreen.h"
#include "screens/HistoryScreen.h"
#include "screens/MenuScreen.h"
#include "screens/ResultsScreen.h"
#include "screens/SessionDetailScreen.h"
#include "screens/SessionScreen.h"
#include "screens/TerminalTooSmallScreen.h"
#include "screens/TextLibraryScreen.h"
#include "typeit/tui/FrameTicker.h"

namespace typeit::tui {

    namespace {
        constexpr std::int64_t kMillisPerSecond = 1'000;
    }  // namespace

    /// The whole of FTXUI and the whole of the navigation, kept here. The
    /// header opposite names none of it.
    struct TerminalApp::Impl {
        /// Fullscreen: a typing test that scrolls the shell's history away as
        /// it redraws is unusable, and the alternate buffer is what a terminal
        /// application is for.
        /// Configured and, more importantly, put back afterwards. Declared
        /// first so it is destroyed last: the console must still be ours while
        /// FTXUI is tearing its screen down. Off Windows it does nothing.
        ConsoleMode console;

        ftxui::ScreenInteractive screen = ftxui::ScreenInteractive::Fullscreen();
        Dependencies dependencies;
        Keymap keymap;
        ScreenContext context;
        ScreenStack stack;
        TooSmallGate gate;
        /// The screen the run is on, borrowed so the tick can reach it. Null
        /// whenever the top is not a session.
        SessionScreen* session = nullptr;
        /// The library's answer, held only for as long as it takes to start the
        /// run it was chosen for.
        std::optional<core::TextId> chosen_text;
        bool quitting = false;

        explicit Impl(Dependencies given) :
            // `dependencies` is declared first, so it is constructed first and
            // the keymap reads from it rather than from the moved-out argument.
            dependencies{std::move(given)}, keymap{Keymap::from_config(dependencies.config->keys)} {
            context.theme = dependencies.theme;
            context.keymap = &keymap;
            context.config = dependencies.config;
            context.capabilities = dependencies.capabilities;
            context.texts = &dependencies.texts;
            context.load_text = dependencies.load_text;
            context.save_text = dependencies.save_text;
            context.history = dependencies.history;
            context.library = dependencies.library;
            context.size = TerminalSize{.columns = 80, .rows = 24};
        }

        /// The size FTXUI last measured. Read every frame rather than cached,
        /// because that is the whole of the resize story.
        void measure() {
            const ftxui::Dimensions size = ftxui::Terminal::Size();
            context.size = TerminalSize{.columns = static_cast<std::size_t>(size.dimx),
                                        .rows = static_cast<std::size_t>(size.dimy)};
        }

        /// What the menu's text field resolves to. Read at start rather than
        /// held by the menu, so a file edited between choosing it and pressing
        /// start is the file that gets typed.
        [[nodiscard]] std::string text_for(const MenuSelection& selection) const {
            if (chosen_text.has_value() && dependencies.library.records != nullptr) {
                // A text picked from the library beats whatever the menu was
                // showing: it is the more recent and more specific request.
                const core::Result<std::optional<app::TextItem>> item = dependencies.library.records->get(*chosen_text);
                // Named rather than reached through two dereferences:
                // clang-tidy cannot see the `has_value` check through the
                // `Result` wrapping the optional.
                if (item.has_value() && item->has_value()) {
                    const std::optional<app::TextItem>& text = item.value();
                    return text->content;
                }
            }
            if (dependencies.texts.empty() || !dependencies.load_text) {
                return dependencies.text;
            }
            const std::filesystem::path path = selection.text < dependencies.texts.size()
                                                       ? dependencies.texts.at(selection.text).path
                                                       : std::filesystem::path{selection.custom_path};
            core::Result<std::string> loaded = dependencies.load_text(path);
            return loaded ? std::move(*loaded) : std::string{};
        }

        /// Starts a run from what the menu chose.
        void start(const MenuSelection& selection) {
            app::SessionRequest request;
            request.mode = selection.mode;
            // The parameter as JSON, which is what the record carries. The mode
            // itself is resolved by the registry the composition root built.
            request.mode_param = selection.mode == "words" ? R"({"words":)" + std::to_string(selection.words) + "}"
                                                           : R"({"seconds":)" + std::to_string(selection.seconds) + "}";
            // The same numbers the line above records, so the history cannot
            // say "15 seconds" beside a run that lasted the configured thirty.
            request.params.duration = core::Millis{selection.seconds * kMillisPerSecond};
            request.params.words = static_cast<std::size_t>(selection.words);
            request.text = text_for(selection);
            if (request.text.empty()) {
                // The menu refuses to start on an unreadable path, so getting
                // here means the catalogue itself is gone. Staying put with the
                // menu's message on screen beats an empty typing area.
                return;
            }
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

        /// Ends the run, if there is one, and asks the loop to stop.
        ///
        /// On `Impl` rather than only on `TerminalApp` so the event handler can
        /// reach it without a round trip through the public class — and because
        /// everything it touches lives here.
        void stop() {
            if (std::exchange(quitting, true)) {
                return;
            }
            // A run still in progress is saved as abandoned rather than
            // dropped: it happened, and quitting is not a reason to pretend
            // otherwise.
            if (session != nullptr) {
                session->abandon();
                session = nullptr;
            }
            // `ExitLoopClosure` is safe to call when no loop is running: it
            // posts a task the loop reads on its next turn, and a loop that
            // never starts simply never reads it.
            screen.ExitLoopClosure()();
        }

        /// A frame: time passes here and nowhere else.
        void tick() {
            if (session != nullptr) {
                session->on_tick(dependencies.clock->now());
            }
            apply_requests();
        }

        /// The keys that mean the same thing wherever the user is.
        ///
        /// Handled above the stack so no screen can swallow them: a help key
        /// that works everywhere except the one screen somebody is stuck on is
        /// the same as no help key.
        [[nodiscard]] bool handle_global(const ftxui::Event& event) {
            const std::optional<Action> action = keymap.action_for(event);
            if (action == Action::ForceQuit) {
                stop();
                return true;
            }
            if (action == Action::Help && stack.top().title() != "help") {
                stack.push(std::make_shared<HelpScreen>(context));
                return true;
            }
            // Not from inside a run: `ctrl-h` mid-session would leave the timer
            // running behind a screen the typist cannot type into.
            if (action == Action::History && session == nullptr && stack.top().title() != "history") {
                stack.push(std::make_shared<HistoryScreen>(context));
                return true;
            }
            // Same rule as the history: not from inside a run, where the timer
            // would keep going behind a screen the typist cannot type into.
            if (action == Action::TextLibrary && session == nullptr && stack.top().title() != "text library") {
                stack.push(std::make_shared<TextLibraryScreen>(context));
                return true;
            }
            return false;
        }

        /// Everything else, offered to the top screen first.
        void handle_screen(const ftxui::Event& event) {
            const bool handled = stack.on_event(event);
            apply_requests();
            if (handled || keymap.action_for(event) != Action::QuitOrBack) {
                return;
            }
            // Nothing wanted it, so it means "go back". At the root that is
            // refused, which is why leaving is a separate binding.
            if (stack.size() > 1) {
                stack.pop();
            } else {
                stop();
            }
        }

        /// What the run on top asked for when it ended.
        void finish_session(SessionScreen& run) {
            const std::optional<SessionOutcome> outcome = run.take_outcome();
            if (!outcome.has_value()) {
                return;
            }
            // Read before the pop: the result lives in the screen that is
            // about to be destroyed. Copied whole rather than by record alone,
            // so the results screen gets the key stats and error pairs `finish`
            // already computed instead of a second pass over a log it no longer
            // has access to.
            const app::SessionResult result = run.result().value_or(app::SessionResult{});
            const bool again = *outcome == SessionOutcome::Restart;
            session = nullptr;
            stack.pop();

            if (again) {
                start_again();
            } else if (*outcome == SessionOutcome::Finished) {
                stack.push(std::make_shared<ResultsScreen>(context, result));
            }
        }

        /// Whatever the top screen has asked for since the last frame.
        ///
        /// One branch per screen that can ask for something, rather than a
        /// virtual `IScreen::request()`: what each of them wants is a different
        /// type, and an interface wide enough for all of them would be an
        /// interface every screen has to ignore most of.
        void apply_requests() {
            if (stack.empty()) {
                return;
            }
            IScreen& top = stack.top();

            if (auto* const menu = dynamic_cast<MenuScreen*>(&top); menu != nullptr) {
                if (menu->take_start()) {
                    start(menu->selection());
                }
            } else if (auto* const run = dynamic_cast<SessionScreen*>(&top); run != nullptr) {
                finish_session(*run);
            } else if (auto* const history = dynamic_cast<HistoryScreen*>(&top); history != nullptr) {
                if (const std::optional<core::SessionId> opened = history->take_opened(); opened.has_value()) {
                    stack.push(std::make_shared<SessionDetailScreen>(context, *opened));
                }
            } else if (auto* const library = dynamic_cast<TextLibraryScreen*>(&top); library != nullptr) {
                if (const std::optional<core::TextId> chosen = library->take_chosen(); chosen.has_value()) {
                    // Popped first: the run starts from the menu underneath,
                    // which still holds the mode and duration that were chosen.
                    chosen_text = chosen;
                    stack.pop();
                    start_again();
                    chosen_text.reset();
                }
            } else if (auto* const detail = dynamic_cast<SessionDetailScreen*>(&top); detail != nullptr) {
                if (detail->take_back()) {
                    // Popped rather than replaced, so the history underneath
                    // still has its filters and its scroll position — which is
                    // the whole reason the stack is a stack.
                    stack.pop();
                }
            } else if (auto* const results = dynamic_cast<ResultsScreen*>(&top); results != nullptr) {
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
                impl_->tick();
            } else if (!impl_->handle_global(event)) {
                impl_->handle_screen(event);
            }
            // Always: FTXUI redraws on a handled event, and every event here
            // either advanced something or was offered to a screen that may
            // have.
            return true;
        });

        impl_->screen.Loop(with_events);
    }

    void TerminalApp::quit() { impl_->stop(); }

    bool TerminalApp::is_quitting() const noexcept { return impl_->quitting; }

}  // namespace typeit::tui
