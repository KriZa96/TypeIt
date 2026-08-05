// Navigation, as a stack rather than as five booleans.
//
// The cases that matter are the ones the flags could not express: that only
// the top screen hears an event, that a screen pushed from inside an event is
// not applied while the pusher is still on the call stack, and that a popped
// screen is actually gone rather than merely no longer looked at.

#include <cstddef>
#include <ftxui/component/event.hpp>
#include <ftxui/dom/elements.hpp>
#include <functional>
#include <gtest/gtest.h>
#include <memory>
#include <string>
#include <string_view>
#include <utility>

#include "IScreen.h"
#include "ScreenStack.h"

namespace typeit::tui {
    namespace {

        /// A screen that records what it was asked and does what it was told.
        class SpyScreen : public IScreen {
        public:
            explicit SpyScreen(std::string name, bool handles = true) : name_{std::move(name)}, handles_{handles} {}

            [[nodiscard]] ftxui::Element render() override {
                ++renders;
                return ftxui::text(name_);
            }

            [[nodiscard]] bool on_event(ftxui::Event /*event*/) override {
                ++events;
                if (during_event) {
                    during_event();
                }
                return handles_;
            }

            [[nodiscard]] std::string_view title() const override { return name_; }

            /// Run from inside `on_event`, so a test can ask the stack to
            /// change while it is dispatching.
            std::function<void()> during_event;

            std::size_t renders = 0;
            std::size_t events = 0;

        private:
            std::string name_;
            bool handles_;
        };

        std::shared_ptr<SpyScreen> a_screen(std::string name, bool handles = true) {
            return std::make_shared<SpyScreen>(std::move(name), handles);
        }

        ftxui::Event any_key() { return ftxui::Event::Character('x'); }

        // --- Push, pop, replace ---------------------------------------------

        TEST(ScreenStackTest, AFreshStackIsEmpty) {
            const ScreenStack stack;

            EXPECT_TRUE(stack.empty());
            EXPECT_EQ(stack.size(), 0U);
        }

        TEST(ScreenStackTest, PushPutsAScreenOnTop) {
            ScreenStack stack;

            stack.push(a_screen("menu"));
            EXPECT_EQ(stack.top().title(), "menu");

            stack.push(a_screen("session"));
            EXPECT_EQ(stack.top().title(), "session");
            EXPECT_EQ(stack.size(), 2U);
        }

        TEST(ScreenStackTest, PopUncoversWhatWasUnderneath) {
            ScreenStack stack;
            stack.push(a_screen("menu"));
            stack.push(a_screen("session"));

            stack.pop();

            EXPECT_EQ(stack.top().title(), "menu");
            EXPECT_EQ(stack.size(), 1U);
        }

        TEST(ScreenStackTest, ReplaceChangesTheTopWithoutGrowingTheStack) {
            // What "restart" is: a fresh run, without the finished one left
            // underneath it for `back` to return to.
            ScreenStack stack;
            stack.push(a_screen("menu"));
            stack.push(a_screen("results"));

            stack.replace(a_screen("session"));

            EXPECT_EQ(stack.top().title(), "session");
            EXPECT_EQ(stack.size(), 2U);
        }

        TEST(ScreenStackTest, ReplacingOnAnEmptyStackJustPushes) {
            ScreenStack stack;

            stack.replace(a_screen("menu"));

            EXPECT_EQ(stack.size(), 1U);
            EXPECT_EQ(stack.top().title(), "menu");
        }

        TEST(ScreenStackTest, PoppingTheLastScreenIsRefused) {
            // An empty stack renders nothing and answers no key, so a program
            // that popped its last screen would be a black terminal that
            // ignores the keyboard. Leaving is `TerminalApp::quit()`.
            ScreenStack stack;
            stack.push(a_screen("menu"));

            stack.pop();

            EXPECT_EQ(stack.size(), 1U);
            EXPECT_EQ(stack.top().title(), "menu");
        }

        TEST(ScreenStackTest, PoppingAnEmptyStackIsNotACrash) {
            ScreenStack stack;

            stack.pop();

            EXPECT_TRUE(stack.empty());
        }

        // --- Only the top ----------------------------------------------------

        TEST(ScreenStackTest, OnlyTheTopScreenReceivesEvents) {
            ScreenStack stack;
            const std::shared_ptr<SpyScreen> menu = a_screen("menu");
            const std::shared_ptr<SpyScreen> session = a_screen("session");
            stack.push(menu);
            stack.push(session);

            EXPECT_TRUE(stack.on_event(any_key()));

            EXPECT_EQ(session->events, 1U);
            EXPECT_EQ(menu->events, 0U) << "the screen underneath is not listening";
        }

        TEST(ScreenStackTest, OnlyTheTopScreenRenders) {
            ScreenStack stack;
            const std::shared_ptr<SpyScreen> menu = a_screen("menu");
            const std::shared_ptr<SpyScreen> session = a_screen("session");
            stack.push(menu);
            stack.push(session);

            const ftxui::Element frame = stack.render();

            EXPECT_NE(frame, nullptr);
            EXPECT_EQ(session->renders, 1U);
            EXPECT_EQ(menu->renders, 0U);
        }

        TEST(ScreenStackTest, AnUnhandledEventIsReportedAsUnhandled) {
            // So the application can act on a key the screen ignored — the quit
            // binding is the obvious one.
            ScreenStack stack;
            stack.push(a_screen("menu", /*handles=*/false));

            EXPECT_FALSE(stack.on_event(any_key()));
        }

        TEST(ScreenStackTest, AnEventWithNothingPushedIsUnhandled) {
            ScreenStack stack;

            EXPECT_FALSE(stack.on_event(any_key()));
        }

        TEST(ScreenStackTest, RenderingAnEmptyStackIsAFrameRatherThanACrash) {
            ScreenStack stack;

            EXPECT_NE(stack.render(), nullptr);
        }

        // --- Lifetime ---------------------------------------------------------

        TEST(ScreenStackTest, APoppedScreenIsGone) {
            // Not merely "no longer on top": the stack held the only reference,
            // so popping ends the object's life. A dangling reference here
            // would be a use-after-free on the next frame.
            ScreenStack stack;
            stack.push(a_screen("menu"));

            std::weak_ptr<SpyScreen> watched;
            {
                const std::shared_ptr<SpyScreen> session = a_screen("session");
                watched = session;
                stack.push(session);
            }
            ASSERT_FALSE(watched.expired()) << "the stack is holding it";

            stack.pop();

            EXPECT_TRUE(watched.expired());
        }

        TEST(ScreenStackTest, AReplacedScreenIsGone) {
            ScreenStack stack;
            std::weak_ptr<SpyScreen> watched;
            {
                const std::shared_ptr<SpyScreen> first = a_screen("results");
                watched = first;
                stack.push(first);
            }

            stack.replace(a_screen("session"));

            EXPECT_TRUE(watched.expired());
        }

        TEST(ScreenStackTest, DeepNestingWorksAndUnwindsInOrder) {
            ScreenStack stack;
            for (int depth = 0; depth < 10; ++depth) {
                stack.push(a_screen("screen" + std::to_string(depth)));
            }
            EXPECT_EQ(stack.size(), 10U);
            EXPECT_EQ(stack.top().title(), "screen9");

            for (int depth = 9; depth > 0; --depth) {
                EXPECT_EQ(stack.top().title(), "screen" + std::to_string(depth));
                stack.pop();
            }

            EXPECT_EQ(stack.size(), 1U);
            EXPECT_EQ(stack.top().title(), "screen0");
        }

        // --- Deferred mutation -------------------------------------------------

        TEST(ScreenStackTest, APushFromInsideAnEventIsDeferredToTheEndOfTheFrame) {
            // The case that makes this worth a class. Applying immediately
            // would destroy the object whose `on_event` is still running.
            ScreenStack stack;
            const std::shared_ptr<SpyScreen> menu = a_screen("menu");
            stack.push(menu);

            bool top_was_still_the_menu = false;
            menu->during_event = [&] {
                stack.push(a_screen("session"));
                top_was_still_the_menu = stack.top().title() == "menu";
            };

            EXPECT_TRUE(stack.on_event(any_key()));

            EXPECT_TRUE(top_was_still_the_menu) << "the push was applied mid-dispatch";
            EXPECT_EQ(stack.top().title(), "session") << "and never applied at all";
            EXPECT_EQ(stack.size(), 2U);
        }

        TEST(ScreenStackTest, APopFromInsideAnEventIsDeferredToo) {
            // "Back" pressed on a screen: the screen must survive returning
            // from its own event handler.
            ScreenStack stack;
            stack.push(a_screen("menu"));
            const std::shared_ptr<SpyScreen> session = a_screen("session");
            stack.push(session);

            session->during_event = [&] { stack.pop(); };

            EXPECT_TRUE(stack.on_event(any_key()));

            EXPECT_EQ(stack.top().title(), "menu");
            EXPECT_EQ(stack.size(), 1U);
        }

        TEST(ScreenStackTest, AScreenMayReplaceItselfAndStillFinishHandlingTheEvent) {
            // Restart, pressed from the results screen. The replaced screen is
            // still the one that finishes the frame.
            ScreenStack stack;
            const std::shared_ptr<SpyScreen> results = a_screen("results");
            stack.push(results);

            bool finished_the_handler = false;
            results->during_event = [&] {
                stack.replace(a_screen("session"));
                finished_the_handler = true;
            };

            EXPECT_TRUE(stack.on_event(any_key()));

            EXPECT_TRUE(finished_the_handler);
            EXPECT_EQ(stack.top().title(), "session");
            EXPECT_EQ(results->events, 1U);
        }

        TEST(ScreenStackTest, SeveralChangesInOneFrameAreAppliedInOrder) {
            ScreenStack stack;
            const std::shared_ptr<SpyScreen> menu = a_screen("menu");
            stack.push(menu);

            menu->during_event = [&] {
                stack.push(a_screen("session"));
                stack.push(a_screen("results"));
                stack.pop();
            };

            EXPECT_TRUE(stack.on_event(any_key()));

            EXPECT_EQ(stack.size(), 2U);
            EXPECT_EQ(stack.top().title(), "session") << "pushed, pushed, popped";
        }

        TEST(ScreenStackTest, TheDeferralIsVisibleWhileItLasts) {
            ScreenStack stack;
            const std::shared_ptr<SpyScreen> menu = a_screen("menu");
            stack.push(menu);

            bool pending_during_dispatch = false;
            menu->during_event = [&] {
                stack.push(a_screen("session"));
                pending_during_dispatch = stack.has_pending();
            };

            EXPECT_FALSE(stack.has_pending());
            EXPECT_TRUE(stack.on_event(any_key()));

            EXPECT_TRUE(pending_during_dispatch);
            EXPECT_FALSE(stack.has_pending()) << "and nothing is left over";
        }

        TEST(ScreenStackTest, OutsideAnEventAChangeIsImmediate) {
            // The deferral is for the one case that needs it, not a general
            // rule that would make every push arrive a frame late.
            ScreenStack stack;

            stack.push(a_screen("menu"));

            EXPECT_FALSE(stack.has_pending());
            EXPECT_EQ(stack.top().title(), "menu");
        }

    }  // namespace
}  // namespace typeit::tui
