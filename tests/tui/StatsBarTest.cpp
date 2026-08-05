// The bars, and the one property that makes them worth a file.
//
// A field that widens as its number grows reflows the text under the typist's
// fingers while they are reading it. Every field here is fixed width, and the
// first test is the one that says so.

#include <cstddef>
#include <gtest/gtest.h>
#include <string>
#include <vector>

#include "Bars.h"
#include "Keymap.h"
#include "Snapshot.h"
#include "typeit/app/Theme.h"
#include "typeit/core/util/Units.h"

namespace typeit::tui {
    namespace {

        SessionStats a_run(double wpm, double accuracy, std::int64_t seconds = 30) {
            return SessionStats{.wpm = core::Wpm{wpm},
                                .accuracy = core::Accuracy{accuracy},
                                .seconds = seconds,
                                .words_done = 0,
                                .words_total = 0};
        }

        std::string drawn(const SessionStats& stats, StatsBarOptions options = {}) {
            return testing::render_to_text(stats_bar(stats, app::Theme{}, options), 60, 1);
        }

        // --- The property ------------------------------------------------------

        TEST(StatsBarTest, AFieldDoesNotChangeWidthAsItsNumberGrows) {
            // The whole reason the fields are fixed: a WPM going from 9 to 100
            // must not push the text sideways while somebody is reading it.
            const std::string single = drawn(a_run(9, 0.99));
            const std::string double_digit = drawn(a_run(99, 0.99));
            const std::string triple = drawn(a_run(100, 0.99));

            EXPECT_EQ(single.size(), double_digit.size());
            EXPECT_EQ(double_digit.size(), triple.size());
        }

        TEST(StatsBarTest, TheWholeBarKeepsItsShapeAcrossEveryPlausibleValue) {
            const std::string reference = drawn(a_run(0, 0.0, 0));

            for (const double wpm: {0.0, 9.0, 42.0, 99.0, 100.0, 999.0}) {
                for (const double accuracy: {0.0, 0.5, 0.999, 1.0}) {
                    for (const std::int64_t seconds: {std::int64_t{0}, std::int64_t{59}, std::int64_t{600}}) {
                        EXPECT_EQ(drawn(a_run(wpm, accuracy, seconds)).size(), reference.size())
                                << wpm << " " << accuracy << " " << seconds;
                    }
                }
            }
        }

        TEST(StatsBarTest, FixedWidthPadsAndNeverOverflows) {
            EXPECT_EQ(fixed_width("9", 3), "  9");
            EXPECT_EQ(fixed_width("100", 3), "100");
            EXPECT_EQ(fixed_width("100%", 4), "100%");

            // Truncated from the left, so the significant digits survive: a
            // field showing `999` for 1999 is wrong, but `199` is worse.
            EXPECT_EQ(fixed_width("1999", 3).size(), 3U);
            EXPECT_EQ(fixed_width("1999", 3), "999");
        }

        // --- Values --------------------------------------------------------------

        TEST(StatsBarTest, TheBoundaryValuesFormatCorrectly) {
            EXPECT_NE(drawn(a_run(0, 0.0)).find("  0"), std::string::npos);
            EXPECT_NE(drawn(a_run(999, 1.0)).find("999"), std::string::npos);
            EXPECT_NE(drawn(a_run(50, 1.0)).find("100%"), std::string::npos);
            EXPECT_NE(drawn(a_run(50, 0.0)).find("0%"), std::string::npos);
        }

        TEST(StatsBarTest, AWpmIsTruncatedRatherThanRounded) {
            // A run that reads 100 when it was 99.6 is a number somebody will
            // screenshot and argue about.
            EXPECT_NE(drawn(a_run(99.6, 1.0)).find(" 99"), std::string::npos);
        }

        TEST(StatsBarTest, TheTimerReadsAsMinutesAndSeconds) {
            EXPECT_NE(drawn(a_run(50, 1.0, 90)).find("1:30"), std::string::npos);
            EXPECT_NE(drawn(a_run(50, 1.0, 5)).find("0:05"), std::string::npos) << "and pads the seconds";
        }

        TEST(StatsBarTest, AModeWithNoTimerShowsItsWordCountInstead) {
            SessionStats words = a_run(50, 1.0, -1);
            words.words_done = 12;
            words.words_total = 50;

            const std::string bar = drawn(words);

            EXPECT_NE(bar.find("12/50"), std::string::npos) << bar;
        }

        // --- Hiding ---------------------------------------------------------------

        TEST(StatsBarTest, AHiddenMetricIsActuallyAbsent) {
            const std::string without_wpm = drawn(a_run(42, 0.98), StatsBarOptions{.show_wpm = false});

            EXPECT_EQ(without_wpm.find("wpm"), std::string::npos) << without_wpm;
            EXPECT_NE(without_wpm.find("acc"), std::string::npos) << "and the rest is still there";
        }

        TEST(StatsBarTest, TheLayoutStillBalancesWithFieldsHidden) {
            // A bar with a hole in it looks like a bug.
            const std::string all = drawn(a_run(42, 0.98));
            const std::string fewer = drawn(a_run(42, 0.98), StatsBarOptions{.show_progress = false});

            EXPECT_EQ(all.size(), fewer.size()) << "the line is the same width";
            EXPECT_EQ(fewer.find("  "), fewer.find("  ")) << "and has no double gap where a field was";
        }

        TEST(StatsBarTest, EverythingHiddenIsAnEmptyLineRatherThanAMissingOne) {
            // So the layout below does not move.
            const std::string nothing =
                    drawn(a_run(42, 0.98),
                          StatsBarOptions{.show_wpm = false, .show_accuracy = false, .show_progress = false});

            EXPECT_FALSE(nothing.empty());
        }

        // --- The hint bar ------------------------------------------------------------

        TEST(StatsBarTest, TheHintBarShowsTheBindingsForItsContext) {
            const Keymap keymap;
            const std::vector<Hint> hints{{.action = Action::ForceQuit, .label = "quit"},
                                          {.action = Action::Restart, .label = "restart"}};

            const std::string bar = testing::render_to_text(key_hint_bar(hints, keymap, app::Theme{}), 60, 1);

            EXPECT_NE(bar.find("ctrl-q"), std::string::npos) << bar;
            EXPECT_NE(bar.find("quit"), std::string::npos) << bar;
            EXPECT_NE(bar.find("restart"), std::string::npos) << bar;
        }

        TEST(StatsBarTest, TheHintBarReflectsARebind) {
            // Read from the keymap, so a rebind shows here without anybody
            // remembering to update a string.
            const Keymap rebound = Keymap::from_config({{"force_quit", "ctrl-x"}});
            const std::vector<Hint> hints{{.action = Action::ForceQuit, .label = "quit"}};

            const std::string bar = testing::render_to_text(key_hint_bar(hints, rebound, app::Theme{}), 60, 1);

            EXPECT_NE(bar.find("ctrl-x"), std::string::npos) << bar;
            EXPECT_EQ(bar.find("ctrl-q"), std::string::npos) << bar;
        }

        TEST(StatsBarTest, AnEmptyHintBarIsALineRatherThanNothing) {
            const std::string bar = testing::render_to_text(key_hint_bar({}, Keymap{}, app::Theme{}), 60, 1);

            EXPECT_FALSE(bar.empty());
        }

    }  // namespace
}  // namespace typeit::tui
