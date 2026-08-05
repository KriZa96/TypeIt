#include "typeit/tui/FrameTicker.h"

#include <chrono>
#include <mutex>
#include <utility>

namespace typeit::tui {

    FrameTicker::FrameTicker(Post post, FrameIntervals intervals) :
        post_{std::move(post)}, intervals_{intervals}, thread_{[this] { loop(); }} {}

    FrameTicker::~FrameTicker() { stop(); }

    std::chrono::milliseconds FrameTicker::interval() const { return active_ ? intervals_.active : intervals_.idle; }

    void FrameTicker::loop() {
        std::unique_lock lock{mutex_};
        auto due = std::chrono::steady_clock::now() + interval();

        while (!stopping_) {
            // `wait_until` returns true when the predicate holds, which is
            // either "stop" or "the rate changed" — neither of which is a
            // frame. It returns false on timeout, and a timeout is the only
            // thing that posts.
            if (wake_.wait_until(lock, due, [this] { return stopping_ || rate_changed_; })) {
                if (stopping_) {
                    return;
                }
                rate_changed_ = false;
                due = std::chrono::steady_clock::now() + interval();
                continue;
            }

            // Unlocked around the callback, so a post that reaches back into
            // the ticker — `set_active` from inside a frame is an obvious
            // thing to want — cannot deadlock it.
            lock.unlock();
            post_();
            lock.lock();

            // Measured from now rather than from the deadline that just
            // passed: a slow frame should not be followed by a burst of
            // catch-up frames nobody can see.
            due = std::chrono::steady_clock::now() + interval();
        }
    }

    void FrameTicker::set_active(bool active) {
        {
            const std::scoped_lock lock{mutex_};
            if (active_ == active) {
                return;
            }
            active_ = active;
            rate_changed_ = true;
        }
        wake_.notify_one();
    }

    bool FrameTicker::is_active() const {
        const std::scoped_lock lock{mutex_};
        return active_;
    }

    void FrameTicker::stop() {
        {
            const std::scoped_lock lock{mutex_};
            stopping_ = true;
        }
        wake_.notify_one();
        if (thread_.joinable()) {
            thread_.join();
        }
    }

    bool FrameTicker::is_running() const { return thread_.joinable(); }

}  // namespace typeit::tui
