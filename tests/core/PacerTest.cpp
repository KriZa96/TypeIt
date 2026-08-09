// The ghost cursor (TI-121).
//
// The lead — the gap between typist and pacer — is the one number the whole
// ramp law reads, so an error here is an error in every decision race mode
// makes. The arithmetic is one line; the cases below are about the ways time
// arrives at it.

#include <cstdint>
#include <gtest/gtest.h>

#include "typeit/core/race/Pacer.h"
#include "typeit/core/util/Units.h"

namespace typeit::core {
    namespace {

        /// No opening grace, for the cases that are about movement rather than
        /// about waiting.
        [[nodiscard]] Pacer moving_at(double wpm) { return Pacer{Wpm{wpm}, Millis{0}}; }

        /// Establishes the origin and then runs `ms` forward in one step.
        void run_for(Pacer& pacer, std::int64_t ms) {
            pacer.advance(Millis{0});
            pacer.advance(Millis{ms});
        }

        // ---- the arithmetic ------------------------------------------------------

        TEST(PacerTest, SixtySecondsAtSixtyWordsAMinuteIsThreeHundredGraphemes) {
            // Hand-verified against the five-graphemes-per-word convention:
            // 60 wpm is 300 graphemes a minute, and this is one minute.
            Pacer pacer = moving_at(60.0);

            run_for(pacer, 60'000);

            EXPECT_DOUBLE_EQ(pacer.position(), 300.0);
        }

        TEST(PacerTest, TheFirstTickEstablishesTheOriginRatherThanCoveringTheEpoch) {
            // Otherwise a pacer handed a wall-clock timestamp covers forty-five
            // thousand years of graphemes on its first call.
            Pacer pacer = moving_at(60.0);

            pacer.advance(Millis{1'700'000'000'000});

            EXPECT_DOUBLE_EQ(pacer.position(), 0.0);
        }

        TEST(PacerTest, ZeroSpeedDoesNotAdvance) {
            Pacer pacer = moving_at(0.0);

            run_for(pacer, 60'000);

            EXPECT_DOUBLE_EQ(pacer.position(), 0.0);
        }

        TEST(PacerTest, ANegativeSpeedIsReadAsStationaryRatherThanAsReverse) {
            // The ramp law clamps to `V_min` and nothing else writes this, so a
            // negative is a caller's bug — and the safe reading of a bug is
            // "does not move", not "runs backwards through the text".
            Pacer pacer{Wpm{-60.0}, Millis{0}};

            run_for(pacer, 60'000);

            EXPECT_DOUBLE_EQ(pacer.position(), 0.0);
            EXPECT_DOUBLE_EQ(pacer.speed().value, 0.0);
        }

        // ---- the grace period ----------------------------------------------------

        TEST(PacerTest, TheGracePeriodHoldsThePacerStillForExactlyItsDuration) {
            Pacer pacer{Wpm{60.0}, Millis{5'000}};

            pacer.advance(Millis{0});
            pacer.advance(Millis{4'999});
            EXPECT_DOUBLE_EQ(pacer.position(), 0.0) << "one millisecond of grace left";
            EXPECT_TRUE(pacer.waiting());

            pacer.advance(Millis{5'000});

            EXPECT_DOUBLE_EQ(pacer.position(), 0.0) << "and the last one is still grace";
            EXPECT_FALSE(pacer.waiting());
        }

        TEST(PacerTest, TheRemainderOfTheStepTheGraceEndsInStillCounts) {
            // Throwing it away would make the start depend on how often
            // somebody happened to call advance, so a race under a 60 Hz
            // frame ticker would begin at a different place than one under 30.
            Pacer coarse{Wpm{60.0}, Millis{5'000}};
            Pacer fine{Wpm{60.0}, Millis{5'000}};

            coarse.advance(Millis{0});
            coarse.advance(Millis{6'000});
            fine.advance(Millis{0});
            for (std::int64_t at = 100; at <= 6'000; at += 100) {
                fine.advance(Millis{at});
            }

            EXPECT_DOUBLE_EQ(coarse.position(), 5.0) << "one second of movement at 60 wpm";
            EXPECT_NEAR(fine.position(), coarse.position(), 1e-9) << "and the step size does not change it";
        }

        // ---- time that misbehaves -------------------------------------------------

        TEST(PacerTest, TimeGoingBackwardsAdvancesNothing) {
            // A suspend or an NTP step must not teleport the pacer through the
            // text and end a run the typist was winning.
            Pacer pacer = moving_at(60.0);
            run_for(pacer, 10'000);
            const double reached = pacer.position();

            pacer.advance(Millis{5'000});

            EXPECT_DOUBLE_EQ(pacer.position(), reached);
        }

        TEST(PacerTest, TheSameInstantTwiceAdvancesNothing) {
            Pacer pacer = moving_at(60.0);
            run_for(pacer, 10'000);
            const double reached = pacer.position();

            pacer.advance(Millis{10'000});

            EXPECT_DOUBLE_EQ(pacer.position(), reached);
        }

        // ---- changing speed --------------------------------------------------------

        TEST(PacerTest, ASpeedChangeAppliesFromThatMomentRatherThanRetroactively) {
            // Recomputing the distance already covered would make a mid-run
            // ramp adjustment move the pacer, which is a jump the typist sees
            // and cannot explain.
            Pacer pacer = moving_at(60.0);
            run_for(pacer, 60'000);
            ASSERT_DOUBLE_EQ(pacer.position(), 300.0);

            pacer.set_speed(Wpm{120.0});
            pacer.advance(Millis{120'000});

            EXPECT_DOUBLE_EQ(pacer.position(), 300.0 + 600.0) << "the first minute stays at the first speed";
        }

        // ---- push-back ---------------------------------------------------------------

        TEST(PacerTest, PushBackPlacesThePacerExactlyThatFarBehindThePlayer) {
            Pacer pacer = moving_at(60.0);
            run_for(pacer, 60'000);

            pacer.push_back(GraphemeIndex{400}, 25);

            EXPECT_DOUBLE_EQ(pacer.position(), 375.0);
            EXPECT_DOUBLE_EQ(pacer.lead(GraphemeIndex{400}), 25.0);
        }

        TEST(PacerTest, PushBackNearTheStartStopsAtTheStart) {
            // A negative position would be an index every reader has to defend
            // against, for a case that means "the very beginning".
            Pacer pacer = moving_at(60.0);
            run_for(pacer, 2'000);

            pacer.push_back(GraphemeIndex{3}, 25);

            EXPECT_DOUBLE_EQ(pacer.position(), 0.0);
        }

        TEST(PacerTest, ThePositionNeverDecreasesExceptByPushBack) {
            Pacer pacer = moving_at(45.0);
            pacer.advance(Millis{0});

            double previous = 0.0;
            for (std::int64_t at = 16; at <= 30'000; at += 16) {
                pacer.advance(Millis{at});
                // Also exercised: a speed change every second, and time
                // standing still every so often.
                if (at % 1'000 < 16) {
                    pacer.set_speed(Wpm{45.0 + static_cast<double>(at) / 1'000.0});
                    pacer.advance(Millis{at});
                }
                EXPECT_GE(pacer.position(), previous) << "at " << at;
                previous = pacer.position();
            }
        }

        // ---- drift -------------------------------------------------------------------

        TEST(PacerTest, TenThousandSmallStepsDoNotDriftFromTheClosedForm) {
            // The reason the position is a double and is only rounded when
            // somebody asks where to draw it. Rounding every step would lose a
            // fraction of a grapheme per frame, which at sixty frames a second
            // is a pacer running slow by more than a word a minute.
            Pacer stepped = moving_at(75.0);
            stepped.advance(Millis{0});
            for (std::int64_t at = 1; at <= 10'000; ++at) {
                stepped.advance(Millis{at});
            }

            Pacer whole = moving_at(75.0);
            run_for(whole, 10'000);

            EXPECT_NEAR(stepped.position(), whole.position(), 1e-9);
            EXPECT_NEAR(stepped.position(), 10'000.0 * 75.0 * 5.0 / 60'000.0, 1e-9);
        }

        TEST(PacerTest, TheDrawnIndexIsTheGraphemeItHasReached) {
            // Truncated rather than rounded: drawing it one ahead of where the
            // lead says it is would put the marker and the number it explains
            // in different places.
            Pacer pacer = moving_at(60.0);
            run_for(pacer, 1'900);

            EXPECT_NEAR(pacer.position(), 9.5, 1e-9);
            EXPECT_EQ(pacer.index(), GraphemeIndex{9});
        }

        // ---- the lead -------------------------------------------------------------------

        TEST(PacerTest, TheLeadIsNegativeWhenThePacerIsAhead) {
            // Which is the state the grace window is counted over, not an
            // error: the typist is behind and has a moment to catch up.
            Pacer pacer = moving_at(60.0);
            run_for(pacer, 60'000);

            EXPECT_DOUBLE_EQ(pacer.lead(GraphemeIndex{250}), -50.0);
        }

    }  // namespace
}  // namespace typeit::core
