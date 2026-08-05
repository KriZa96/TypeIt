#include "ScreenStack.h"

#include <cassert>
#include <ftxui/component/event.hpp>
#include <ftxui/dom/elements.hpp>
#include <memory>
#include <utility>
#include <vector>

namespace typeit::tui {

    void ScreenStack::request(Requested change) {
        if (dispatching_) {
            // The screen asking for this is still on the call stack. Applying
            // now would destroy the object whose `on_event` we are inside.
            pending_.push_back(std::move(change));
            return;
        }
        apply(change);
    }

    void ScreenStack::apply(const Requested& request) {
        switch (request.change) {
            case Change::Push:
                screens_.push_back(request.screen);
                return;
            case Change::Pop:
                if (screens_.size() > 1) {
                    screens_.pop_back();
                }
                return;
            case Change::Replace:
                if (screens_.empty()) {
                    screens_.push_back(request.screen);
                    return;
                }
                screens_.back() = request.screen;
                return;
        }
    }

    void ScreenStack::apply_pending() {
        // Safe to iterate while applying: `dispatching_` is already false
        // here, so nothing `apply` does can queue another change.
        for (const Requested& change: pending_) {
            apply(change);
        }
        pending_.clear();
    }

    void ScreenStack::push(std::shared_ptr<IScreen> screen) {
        assert(screen != nullptr && "a null screen would be dereferenced on the next frame");
        request(Requested{.change = Change::Push, .screen = std::move(screen)});
    }

    void ScreenStack::pop() { request(Requested{.change = Change::Pop, .screen = nullptr}); }

    void ScreenStack::replace(std::shared_ptr<IScreen> screen) {
        assert(screen != nullptr && "a null screen would be dereferenced on the next frame");
        request(Requested{.change = Change::Replace, .screen = std::move(screen)});
    }

    IScreen& ScreenStack::top() {
        assert(!screens_.empty() && "there is no top of an empty stack");
        return *screens_.back();
    }

    const IScreen& ScreenStack::top() const {
        assert(!screens_.empty() && "there is no top of an empty stack");
        return *screens_.back();
    }

    ftxui::Element ScreenStack::render() {
        if (screens_.empty()) {
            return ftxui::text("");
        }
        return screens_.back()->render();
    }

    bool ScreenStack::on_event(ftxui::Event event) {
        if (screens_.empty()) {
            return false;
        }

        // The top screen, read before dispatch: a screen that replaces itself
        // must still be the one that finishes handling the event.
        const std::shared_ptr<IScreen> receiving = screens_.back();

        dispatching_ = true;
        const bool handled = receiving->on_event(std::move(event));
        dispatching_ = false;

        apply_pending();
        return handled;
    }

}  // namespace typeit::tui
