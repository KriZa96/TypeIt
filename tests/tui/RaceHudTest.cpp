// The race strip and the pacer bar (TI-125).
//
// The bar is the primary feedback channel, so the cases are about geometry
// rather than about colours: where the ghost ends, where the caret sits, and
// that the distance between them is the lead drawn to scale. A bar that looks
// right and measures wrong is a bar that lies to somebody mid-race.

#include <cstddef>
#include <functional>
#include <gtest/gtest.h>
#include <string>

#include "RaceHud.h"
#include "Snapshot.h"
#include "typeit/app/Capabilities.h"
#include "typeit/app/Theme.h"
#include "typeit/core/race/DifficultyController.h"
#include "typeit/core/util/Units.h"

namespace typeit::tui {
    namespace {

        constexpr std::size_t kWidth = 40;

        [[nodiscard]] RaceHudState a_race() {
            RaceHudState state;
            state.target = core::Wpm{74.0};
            state.you = core::Wpm{81.0};
            state.accuracy = core::Accuracy{0.981};
            state.lead = 31.0;
            state.lives_left = 2;
            state.lives_total = 3;
            state.pacer = core::GraphemeIndex{300};
            state.player = core::GraphemeIndex{331};
            state.trend = core::RampTrend::Climbing;
            return state;
        }

        [[nodiscard]] std::string drawn(const RaceHudState& state, RaceHudOptions options = {}) {
            const app::Theme theme;
            return testing::render_to_text(pacer_bar(state, theme, kWidth, options), kWidth, 1);
        }

        [[nodiscard]] std::string strip(const RaceHudState& state, RaceHudOptions options = {}) {
            const app::Theme theme;
            return testing::render_to_text(race_stats_bar(state, theme, options), 80, 1);
        }

        // ---- the geometry ------------------------------------------------------------

        TEST(RaceHudTest, TheCaretIsAheadOfTheGhostWhenTheTypistIs) {
            const RaceHudState state = a_race();

            const std::size_t ghost = bar_column(state, kWidth, state.pacer, RaceHudOptions{}.window);
            const std::size_t typist = bar_column(state, kWidth, state.player, RaceHudOptions{}.window);

            EXPECT_LT(ghost, typist) << "the ghost is behind, and the bar has to say so";
        }

        TEST(RaceHudTest, TheGapBetweenThemGrowsWithTheLead) {
            // "The gap between them *is* the lead" — so a bigger lead is a
            // bigger gap, or the bar is decoration.
            RaceHudState near = a_race();
            near.pacer = core::GraphemeIndex{325};
            near.player = core::GraphemeIndex{331};
            RaceHudState far = a_race();
            far.pacer = core::GraphemeIndex{280};
            far.player = core::GraphemeIndex{331};

            const std::size_t near_gap = bar_column(near, kWidth, near.player, RaceHudOptions{}.window) -
                                         bar_column(near, kWidth, near.pacer, RaceHudOptions{}.window);
            const std::size_t far_gap = bar_column(far, kWidth, far.player, RaceHudOptions{}.window) -
                                        bar_column(far, kWidth, far.pacer, RaceHudOptions{}.window);

            EXPECT_GT(far_gap, near_gap);
        }

        TEST(RaceHudTest, ALeadOfNothingPutsThemInTheSameColumn) {
            RaceHudState level = a_race();
            level.pacer = core::GraphemeIndex{331};
            level.player = core::GraphemeIndex{331};
            level.lead = 0.0;

            EXPECT_EQ(bar_column(level, kWidth, level.pacer, RaceHudOptions{}.window),
                      bar_column(level, kWidth, level.player, RaceHudOptions{}.window));
        }

        TEST(RaceHudTest, TheLayoutHoldsWhenTheGhostIsAhead) {
            // A negative lead is an ordinary state for the length of the grace
            // window, and the bar must not fold up or lose its ends.
            RaceHudState behind = a_race();
            behind.pacer = core::GraphemeIndex{360};
            behind.player = core::GraphemeIndex{331};
            behind.lead = -29.0;

            const std::string bar = drawn(behind);

            EXPECT_EQ(bar.front(), '|');
            EXPECT_NE(bar.find('|', 1), std::string::npos) << "both ends are still there: " << bar;
            EXPECT_LT(bar_column(behind, kWidth, behind.player, RaceHudOptions{}.window),
                      bar_column(behind, kWidth, behind.pacer, RaceHudOptions{}.window));
        }

        TEST(RaceHudTest, TheStartOfARaceDrawsWithoutRunningOffTheLeftEnd) {
            RaceHudState fresh = a_race();
            fresh.pacer = core::GraphemeIndex{0};
            fresh.player = core::GraphemeIndex{0};
            fresh.lead = 0.0;

            EXPECT_EQ(bar_column(fresh, kWidth, fresh.pacer, RaceHudOptions{}.window), 0U);
            EXPECT_NO_FATAL_FAILURE(static_cast<void>(drawn(fresh)));
        }

        TEST(RaceHudTest, ABarNarrowerThanItsOwnEndsDrawsNothing) {
            // Rather than a partial bar, which would be worse than none. The
            // responsive layout has a screen for this case.
            const app::Theme theme;

            EXPECT_EQ(testing::render_to_text(pacer_bar(a_race(), theme, 2), 10, 1).find('|'), std::string::npos);
        }

        // ---- the trend, which is the accuracy gate made visible ----------------------------

        TEST(RaceHudTest, TheTrendMarkerSaysWhichWayTheRampIsGoing) {
            // A typist forty graphemes ahead and gaining nothing should be able
            // to see *why* rather than wonder why the number stopped moving.
            RaceHudState climbing = a_race();
            climbing.trend = core::RampTrend::Climbing;
            RaceHudState holding = a_race();
            holding.trend = core::RampTrend::Holding;
            RaceHudState backing = a_race();
            backing.trend = core::RampTrend::BackingOff;

            const std::string up = strip(climbing);
            const std::string flat = strip(holding);
            const std::string down = strip(backing);

            EXPECT_NE(up, flat);
            EXPECT_NE(flat, down);
            EXPECT_NE(up, down);
        }

        // ---- what the strip says -----------------------------------------------------------

        TEST(RaceHudTest, TheStripCarriesBothSpeedsAndTheLead) {
            const std::string bar = strip(a_race());

            EXPECT_NE(bar.find("74"), std::string::npos) << bar;
            EXPECT_NE(bar.find("81"), std::string::npos) << bar;
            EXPECT_NE(bar.find("31"), std::string::npos) << bar;
            EXPECT_NE(bar.find("98%"), std::string::npos) << bar;
        }

        TEST(RaceHudTest, ANegativeLeadIsSignedRatherThanShownAsItsOwnOpposite) {
            RaceHudState behind = a_race();
            behind.lead = -12.0;

            EXPECT_NE(strip(behind).find("-12"), std::string::npos) << strip(behind);
        }

        TEST(RaceHudTest, LivesShowWhatIsLeftAndWhatHasGone) {
            RaceHudState state = a_race();
            state.lives_left = 1;
            state.lives_total = 3;

            const std::string with_one = strip(state);
            state.lives_left = 3;
            const std::string with_three = strip(state);

            EXPECT_NE(with_one, with_three) << "losing a life is visible";
        }

        // ---- the ASCII fallback ---------------------------------------------------------------

        TEST(RaceHudTest, EveryRaceGlyphHasAnAsciiForm) {
            RaceHudOptions ascii;
            ascii.glyphs = app::GlyphSet::Ascii;

            const std::string bar = drawn(a_race(), ascii);
            const std::string top = strip(a_race(), ascii);

            for (const char letter: bar + top) {
                EXPECT_LT(static_cast<unsigned char>(letter), 0x80U) << "a terminal without Unicode got " << bar << top;
            }
        }

        // ---- the assertion every widget owes ------------------------------------------------

        TEST(RaceHudTest, DrawingChangesNothing) {
            const app::Theme theme;
            const RaceHudState state = a_race();

            testing::expect_render_is_pure([&state, &theme] { return pacer_bar(state, theme, kWidth); }, kWidth, 1);
            testing::expect_render_is_pure([&state, &theme] { return race_stats_bar(state, theme); }, 80, 1);
        }

        // ---- snapshots -------------------------------------------------------------------------

        TEST(RaceHudTest, SnapshotAt80x24) {
            const app::Theme theme;
            const ftxui::Element hud = ftxui::vbox({
                    race_stats_bar(a_race(), theme),
                    pacer_bar(a_race(), theme, 78),
            });

            testing::expect_matches_golden("race_hud_80x24", testing::render_to_text(hud, 80, 24));
        }

        TEST(RaceHudTest, SnapshotAt120x40) {
            const app::Theme theme;
            const ftxui::Element hud = ftxui::vbox({
                    race_stats_bar(a_race(), theme),
                    pacer_bar(a_race(), theme, 118),
            });

            testing::expect_matches_golden("race_hud_120x40", testing::render_to_text(hud, 120, 40));
        }

    }  // namespace
}  // namespace typeit::tui
