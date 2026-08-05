// The terminal's lifetime, which is the only thing there is to test before
// there are screens to put on it.
//
// What is deliberately *not* here: entering the event loop. FTXUI's loop reads
// the terminal, and a test that starts it in CI is a test that hangs a
// pipeline on the day stdin behaves differently. The shutdown path is reachable
// without it, and the loop itself is exercised by a person running the program.

#include <gtest/gtest.h>

#include "typeit/tui/TerminalApp.h"

namespace typeit::tui {
    namespace {

        TEST(TerminalAppTest, ConstructsAndDestructsCleanly) {
            // Constructing takes the terminal over and destructing hands it
            // back. Under ASan this is also the leak check: FTXUI's screen
            // holds an installed signal handler and a restored terminal mode.
            const TerminalApp app;

            EXPECT_FALSE(app.is_quitting());
        }

        TEST(TerminalAppTest, RepeatedConstructionIsSafe) {
            // 1.0 kept the screen in a class that was constructed once and
            // never again, so nothing ever proved a second one was possible.
            // A restart pops and pushes, which means it has to be.
            for (int i = 0; i < 10; ++i) {
                const TerminalApp app;
                EXPECT_FALSE(app.is_quitting());
            }
        }

        TEST(TerminalAppTest, QuitBeforeRunIsHonouredWithoutEnteringTheLoop) {
            // "Stop" arriving before "start" is not a special case for tests:
            // entering the loop to leave it again would still take the terminal
            // over and hand it back, which is a visible flicker for no reason.
            TerminalApp app;

            app.quit();
            EXPECT_TRUE(app.is_quitting());

            app.run();  // Returns immediately, or this test hangs.
            EXPECT_TRUE(app.is_quitting());
        }

        TEST(TerminalAppTest, QuittingTwiceIsQuittingOnce) {
            TerminalApp app;

            app.quit();
            app.quit();

            EXPECT_TRUE(app.is_quitting());
        }

        TEST(TerminalAppTest, TwoApplicationsDoNotShareState) {
            // The regression guard for the global screen: quitting one must not
            // quit the other.
            TerminalApp first;
            const TerminalApp second;

            first.quit();

            EXPECT_TRUE(first.is_quitting());
            EXPECT_FALSE(second.is_quitting());
        }

    }  // namespace
}  // namespace typeit::tui
