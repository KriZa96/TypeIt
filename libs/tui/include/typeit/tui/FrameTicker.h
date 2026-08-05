// The one background thread, and everything it is allowed to do (ADR-012).
//
// It posts. That is all. It holds no model type, reads no state and decides
// nothing about what a frame contains — which is what makes the domain
// single-threaded by contract rather than by hope, and what makes a whole
// category of race unrepresentable instead of merely absent today.
//
// It replaces 1.0's `Screen` thread, which polled every 100 ms whether or not
// anything was happening. A menu redrawing ten times a second is a laptop
// running its fan for nothing; a run redrawing ten times a second is visibly
// coarse. The rate is therefore adaptive: ~60 ms while somebody is typing,
// ~500 ms while nothing is.
//
// Lifetime is RAII and there is no other option. The destructor stops the
// thread and joins it, so a ticker cannot outlive the callback it posts to —
// the defect that a detached thread invites and that nothing in 1.0 prevented.
#ifndef TYPEIT_TUI_FRAMETICKER_H
#define TYPEIT_TUI_FRAMETICKER_H

#include <chrono>
#include <condition_variable>
#include <functional>
#include <mutex>
#include <thread>

namespace typeit::tui {

    /// ADR-012's rates. Parameters rather than constants because a test that
    /// has to wait 500 ms to watch an idle tick is a test nobody runs, and
    /// because a future settings screen is entitled to an opinion.
    ///
    /// At namespace scope rather than nested in `FrameTicker`, which is not a
    /// style choice: a nested type's default member initialisers are not parsed
    /// until the enclosing class is complete, so `Intervals intervals = {}` as
    /// a default argument inside the class body is ill-formed — accepted by
    /// gcc, rejected by clang.
    struct FrameIntervals {
        std::chrono::milliseconds active{60};
        std::chrono::milliseconds idle{500};
    };

    class FrameTicker {
    public:
        /// What a tick does. Called on the ticker's own thread, never with the
        /// ticker's lock held — so it may do anything, including calling back
        /// into the ticker, without deadlocking it.
        ///
        /// In the application this is `ScreenInteractive::PostEvent`, which is
        /// the one FTXUI call documented as safe from another thread.
        using Post = std::function<void()>;

        /// Starts ticking immediately, at the active rate. A ticker that had to
        /// be started separately would be a ticker somebody forgets to start.
        explicit FrameTicker(Post post, FrameIntervals intervals = {});

        /// Stops and joins. Never detaches: a thread outliving the object whose
        /// callback it holds is a use-after-free waiting for a slow machine.
        ~FrameTicker();

        FrameTicker(const FrameTicker&) = delete;
        FrameTicker& operator=(const FrameTicker&) = delete;
        FrameTicker(FrameTicker&&) = delete;
        FrameTicker& operator=(FrameTicker&&) = delete;

        /// Switches rates, and wakes the thread so the change takes effect now
        /// rather than after the interval it is already waiting out. Going from
        /// idle to active on the first keystroke must not take half a second to
        /// be noticed.
        void set_active(bool active);

        [[nodiscard]] bool is_active() const;

        /// Stops and joins early. Idempotent, and what the destructor calls; a
        /// caller that wants the thread gone before the object is has no reason
        /// to be denied.
        void stop();

        /// Whether the thread is still running. False once `stop()` has
        /// returned — and because `stop()` joins, false means the thread is
        /// gone rather than merely asked to leave, which is the whole
        /// difference from the flag this replaces.
        [[nodiscard]] bool is_running() const;

    private:
        void loop();
        [[nodiscard]] std::chrono::milliseconds interval() const;

        Post post_;
        FrameIntervals intervals_;

        mutable std::mutex mutex_;
        std::condition_variable wake_;
        bool active_ = true;
        bool stopping_ = false;
        /// Set by `set_active` so the loop can tell "the rate changed" from
        /// "the interval elapsed" — without it, every rate change would post a
        /// frame nobody asked for.
        bool rate_changed_ = false;

        std::thread thread_;
    };

}  // namespace typeit::tui

#endif  // TYPEIT_TUI_FRAMETICKER_H
