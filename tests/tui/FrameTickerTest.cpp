// The frame thread, tested on posted events rather than on the clock.
//
// 1.0's five `ScreenTest` cases are all here, and between them they slept for
// three seconds to assert a boolean. Waiting on the thing the object actually
// does — a post — is both exact and instant: this file finishes in a few
// milliseconds, and it fails when the ticker stops ticking rather than when a
// loaded machine misses a deadline.
//
// **No `sleep_for` anywhere.** A test that sleeps is slow when it passes and
// mystifying when it does not.

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstddef>
#include <gtest/gtest.h>
#include <memory>
#include <mutex>
#include <thread>

#include "typeit/tui/FrameTicker.h"

namespace typeit::tui {
    namespace {

        using namespace std::chrono_literals;

        /// Intervals small enough that the whole file is over before a legacy
        /// test had finished its first sleep. The ratio is what matters, not
        /// the numbers: idle must be far enough from active to be told apart.
        constexpr FrameIntervals kFast{.active = 1ms, .idle = 10ms};

        /// No interval at all, for the cases that care about the thread rather
        /// than the rate. The ticker posts as fast as it can and is destroyed a
        /// moment later, so nothing spins for long.
        constexpr FrameIntervals kInstant{.active = 0ms, .idle = 0ms};

        /// Counts posts and lets a test wait for the nth one.
        ///
        /// This is the countdown latch the issue asks for, with a count rather
        /// than a one-shot flag — a test that wants "five frames" should say
        /// so, and `std::latch` cannot be re-armed.
        class Posts {
        public:
            void record() {
                {
                    const std::scoped_lock lock{mutex_};
                    ++count_;
                }
                arrived_.notify_all();
            }

            [[nodiscard]] std::size_t count() const {
                const std::scoped_lock lock{mutex_};
                return count_;
            }

            /// Blocks until at least `wanted` posts have arrived. Returns false
            /// if the ticker went silent, so a broken ticker fails the test
            /// instead of hanging the suite.
            [[nodiscard]] bool wait_for_at_least(std::size_t wanted) {
                std::unique_lock lock{mutex_};
                return arrived_.wait_for(lock, 10s, [&] { return count_ >= wanted; });
            }

            [[nodiscard]] FrameTicker::Post post() {
                return [this] { record(); };
            }

        private:
            mutable std::mutex mutex_;
            std::condition_variable arrived_;
            std::size_t count_ = 0;
        };

        // --- The five cases 1.0 slept through --------------------------------

        // Ported from ScreenTest.TestIsRunning.
        TEST(FrameTickerTest, ConstructionStartsTheThread) {
            Posts posts;
            const FrameTicker ticker{posts.post(), kFast};

            EXPECT_TRUE(ticker.is_running());
            EXPECT_TRUE(posts.wait_for_at_least(1)) << "a ticker that never ticks is not running";
        }

        // Ported from ScreenTest.TestIsRunningAfterSomeTime. "Some time" is now
        // "ten frames", which is the thing the test was actually about.
        TEST(FrameTickerTest, ItKeepsTicking) {
            Posts posts;
            const FrameTicker ticker{posts.post(), kFast};

            EXPECT_TRUE(posts.wait_for_at_least(10));
            EXPECT_TRUE(ticker.is_running());
        }

        // Ported from ScreenTest.TestIsStopped.
        TEST(FrameTickerTest, StoppingEndsTheThread) {
            Posts posts;
            FrameTicker ticker{posts.post(), kFast};

            ticker.stop();

            EXPECT_FALSE(ticker.is_running());
        }

        // Ported from ScreenTest.TestIsStoppedAfterSomeTime.
        TEST(FrameTickerTest, StoppingAfterItHasBeenTickingEndsTheThread) {
            Posts posts;
            FrameTicker ticker{posts.post(), kFast};
            ASSERT_TRUE(posts.wait_for_at_least(5));

            ticker.stop();

            EXPECT_FALSE(ticker.is_running());
        }

        // Ported from ScreenTest.TestIsRunningAndStoppedAfterSomeTime.
        TEST(FrameTickerTest, NothingIsPostedOnceItHasStopped) {
            // The assertion 1.0's flag could not make. `stop()` **joins**, so
            // by the time it returns the thread is gone rather than merely
            // asked to leave — which is why reading the count twice is exact
            // rather than a race.
            Posts posts;
            FrameTicker ticker{posts.post(), kFast};
            ASSERT_TRUE(posts.wait_for_at_least(3));

            ticker.stop();
            const std::size_t at_stop = posts.count();

            EXPECT_FALSE(ticker.is_running());
            EXPECT_EQ(posts.count(), at_stop);
        }

        // --- The rate ---------------------------------------------------------

        TEST(FrameTickerTest, ItStartsActive) {
            Posts posts;
            const FrameTicker ticker{posts.post(), kFast};

            EXPECT_TRUE(ticker.is_active()) << "a run begins when the screen opens, not when a flag is set";
        }

        TEST(FrameTickerTest, GoingIdleLengthensTheInterval) {
            // Counted over a window, as the issue asks: the wait that yields
            // one idle frame would have yielded about ten active ones, because
            // idle is ten times longer.
            Posts posts;
            FrameTicker ticker{posts.post(), kFast};
            ASSERT_TRUE(posts.wait_for_at_least(5));

            ticker.set_active(false);
            EXPECT_FALSE(ticker.is_active());
            const std::size_t at_idle = posts.count();

            // One idle frame, and then read how many arrived while waiting for
            // it. At the active rate this window would have produced ten.
            ASSERT_TRUE(posts.wait_for_at_least(at_idle + 1));
            EXPECT_LE(posts.count() - at_idle, 4U) << "the interval did not lengthen";
        }

        TEST(FrameTickerTest, GoingActiveIsNoticedWithoutWaitingOutTheIdleInterval) {
            // The reason `set_active` wakes the thread: the first keystroke
            // after a pause must not take the rest of an idle interval to
            // reach the screen.
            Posts posts;
            FrameTicker ticker{posts.post(), {.active = 1ms, .idle = 5s}};
            ticker.set_active(false);
            const std::size_t at_idle = posts.count();

            ticker.set_active(true);

            // Without the wake this would sit out five seconds and the wait
            // would give up.
            EXPECT_TRUE(posts.wait_for_at_least(at_idle + 2));
        }

        TEST(FrameTickerTest, SettingTheRateItAlreadyHasChangesNothing) {
            Posts posts;
            FrameTicker ticker{posts.post(), kFast};

            ticker.set_active(true);

            EXPECT_TRUE(ticker.is_active());
            EXPECT_TRUE(posts.wait_for_at_least(1));
        }

        // --- Lifetime ---------------------------------------------------------

        TEST(FrameTickerTest, DestructionStopsAndJoins) {
            Posts posts;
            {
                const FrameTicker ticker{posts.post(), kFast};
                ASSERT_TRUE(posts.wait_for_at_least(3));
            }
            const std::size_t at_destruction = posts.count();

            // If the destructor had detached rather than joined, the thread
            // would still be posting into a `Posts` that is about to go out of
            // scope — which is the use-after-free this asserts is impossible.
            EXPECT_EQ(posts.count(), at_destruction);
        }

        TEST(FrameTickerTest, StoppingTwiceIsStoppingOnce) {
            Posts posts;
            FrameTicker ticker{posts.post(), kFast};

            ticker.stop();
            ticker.stop();

            EXPECT_FALSE(ticker.is_running());
        }

        TEST(FrameTickerTest, DestructionWhileAPostIsInFlightIsSafe) {
            // The race a detached thread loses. The post is held inside the
            // callback while the destructor runs on another thread; the
            // destructor must wait for it rather than pull the object out from
            // under it.
            std::atomic<bool> released{false};
            std::atomic<bool> in_post{false};

            auto ticker = std::make_unique<FrameTicker>(
                    [&] {
                        in_post = true;
                        while (!released) {
                            std::this_thread::yield();  // Not a sleep: it spins until told.
                        }
                    },
                    kFast);

            while (!in_post) {
                std::this_thread::yield();
            }

            std::thread destroyer{[&] { ticker.reset(); }};
            released = true;
            destroyer.join();

            EXPECT_EQ(ticker, nullptr);
        }

        TEST(FrameTickerTest, AHundredCyclesLeakNothing) {
            // Under ASan and TSan this is the leak and race check; without them
            // it is still the check that a thread is joined every time rather
            // than most times.
            for (int cycle = 0; cycle < 100; ++cycle) {
                Posts posts;
                const FrameTicker ticker{posts.post(), kInstant};
                ASSERT_TRUE(posts.wait_for_at_least(1)) << "cycle " << cycle;
            }
        }

        TEST(FrameTickerTest, ThePostMayReachBackIntoTheTicker) {
            // The callback runs without the ticker's lock held, so a frame that
            // decides the run has gone idle can say so from inside itself.
            std::atomic<FrameTicker*> ticker_address{nullptr};
            Posts posts;

            FrameTicker ticker{[&] {
                                   posts.record();
                                   if (FrameTicker* self = ticker_address; self != nullptr) {
                                       self->set_active(false);  // Would deadlock if the lock were held.
                                   }
                               },
                               kInstant};
            ticker_address = &ticker;

            EXPECT_TRUE(posts.wait_for_at_least(2));
        }

    }  // namespace
}  // namespace typeit::tui
