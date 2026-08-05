// Navigation as a stack, because that is what it always was (TECHNICAL §3.2).
//
// 1.0 kept it in five booleans on a shared `GameState` —
// `game_session_in_progress`, `start_session`, `refresh_session`,
// `game_finished`, `show_info` — any of which could be flipped from three
// layers away, and whose sixteen unreachable combinations were unreachable
// only by convention. "Restart" was a flag another frame noticed; "back" was a
// flag being cleared.
//
// Here it is one stack. Push to go somewhere, pop to come back, replace to go
// somewhere instead. The top renders and the top receives events; nothing else
// does, and there is no flag to disagree with.
//
// **Mutation during dispatch is deferred.** A screen that pushes another from
// inside `on_event` is the ordinary case — pressing Enter in a menu starts a
// run — and applying it immediately would destroy the object whose method is
// still on the stack. So push, pop and replace queue while an event is being
// dispatched and are applied when it returns, in the order they were asked
// for.
#ifndef TYPEIT_TUI_SCREENSTACK_H
#define TYPEIT_TUI_SCREENSTACK_H

#include <cstddef>
#include <cstdint>
#include <ftxui/component/event.hpp>
#include <ftxui/dom/elements.hpp>
#include <memory>
#include <vector>

#include "IScreen.h"

namespace typeit::tui {

    class ScreenStack {
    public:
        /// A null screen is refused rather than stored: the top of the stack is
        /// dereferenced on every frame, and a crash there is a crash with no
        /// clue in it.
        void push(std::shared_ptr<IScreen> screen);

        /// Removes the top screen. **Refused when it is the only one**: an
        /// empty stack renders nothing and answers no key, so a program that
        /// popped its last screen would be a black terminal that ignores the
        /// keyboard. Leaving is `TerminalApp::quit()`, which says so.
        void pop();

        /// Pops and pushes as one step, for a "restart" that must not leave the
        /// finished run underneath it. Replacing the only screen is allowed —
        /// the stack does not shrink.
        void replace(std::shared_ptr<IScreen> screen);

        [[nodiscard]] bool empty() const noexcept { return screens_.empty(); }
        [[nodiscard]] std::size_t size() const noexcept { return screens_.size(); }

        /// Precondition: not empty. A caller with nothing pushed has nothing to
        /// ask about.
        [[nodiscard]] IScreen& top();
        [[nodiscard]] const IScreen& top() const;

        /// The top screen's frame, or an empty element when there is no screen
        /// — which is a state the application passes through once, on the way
        /// to pushing its first.
        [[nodiscard]] ftxui::Element render();

        /// Delivers to the top screen and then applies whatever it asked for.
        /// Returns what the screen returned, or `false` when the stack is
        /// empty.
        [[nodiscard]] bool on_event(ftxui::Event event);

        /// Whether a change is waiting for the current dispatch to finish.
        /// Exposed so a test can assert the deferral rather than infer it.
        [[nodiscard]] bool has_pending() const noexcept { return !pending_.empty(); }

    private:
        enum class Change : std::uint8_t {
            Push,
            Pop,
            Replace,
        };

        struct Requested {
            Change change = Change::Push;
            /// Null for a pop.
            std::shared_ptr<IScreen> screen;
        };

        void apply(const Requested& request);
        void apply_pending();
        void request(Requested change);

        std::vector<std::shared_ptr<IScreen>> screens_;
        std::vector<Requested> pending_;
        bool dispatching_ = false;
    };

}  // namespace typeit::tui

#endif  // TYPEIT_TUI_SCREENSTACK_H
