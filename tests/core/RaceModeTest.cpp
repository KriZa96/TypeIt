// Lives, grace and being caught (TI-124).
//
// Fully deterministic: time is a parameter everywhere below this, so a race
// that takes a minute to lose takes microseconds to test. Nothing here sleeps
// and nothing here reads a clock.
//
// The grace window is the case that matters most. A single fumbled keystroke
// must not end a run and a sustained inability to keep up must, and the only
// difference between those two is a duration — so it is tested at, just below,
// and just above the threshold rather than somewhere comfortably either side.

#include <algorithm>
#include <cstdint>
#include <gtest/gtest.h>
#include <string>

#include "typeit/core/modes/RaceMode.h"
#include "typeit/core/race/RaceParams.h"
#include "typeit/core/session/Keystroke.h"
#include "typeit/core/session/TypingModel.h"
#include "typeit/core/text/TextBuffer.h"
#include "typeit/core/util/Result.h"
#include "typeit/core/util/Units.h"

namespace typeit::core {
    namespace {

        /// Long enough that no case below runs out of text.
        [[nodiscard]] const TextBuffer& long_text() {
            static const TextBuffer buffer = [] {
                std::string text;
                while (text.size() < 4'000) {
                    text += "the quick brown fox jumps over the lazy dog. ";
                }
                Result<TextBuffer> built = TextBuffer::from_utf8(text);
                EXPECT_TRUE(built);
                return std::move(*built);
            }();
            return buffer;
        }

        /// One grapheme the text never contains, for the fumble cases. Built
        /// through a buffer because that is the only way to make one, and
        /// cached because it is the same every time.
        [[nodiscard]] Grapheme a_wrong_grapheme() {
            static const Grapheme wrong = [] {
                Result<TextBuffer> built = TextBuffer::from_utf8("~");
                EXPECT_TRUE(built);
                return built->at(GraphemeIndex{0});
            }();
            return wrong;
        }

        /// A model over that text, plus the driving needed to move its cursor.
        class Typist {
        public:
            Typist() : model_{long_text()} {}

            /// Types `count` graphemes correctly, at `at`, telling the mode
            /// about each one.
            void type(RaceMode& mode, std::size_t count, Millis at) {
                for (std::size_t done = 0; done < count; ++done) {
                    const GraphemeIndex target = model_.cursor();
                    const Grapheme expected = long_text().at(target);
                    model_.type(expected, at);
                    mode.on_keystroke(Keystroke{.at = at,
                                                .target = static_cast<std::uint32_t>(target.value),
                                                .kind = KeystrokeKind::Character,
                                                .typed = expected},
                                      model_);
                }
            }

            /// Types `count` graphemes wrongly, which is what moves the
            /// accuracy gate.
            void fumble(RaceMode& mode, std::size_t count, Millis at) {
                const Grapheme wrong = a_wrong_grapheme();
                for (std::size_t done = 0; done < count; ++done) {
                    const GraphemeIndex target = model_.cursor();
                    model_.type(wrong, at);
                    mode.on_keystroke(Keystroke{.at = at,
                                                .target = static_cast<std::uint32_t>(target.value),
                                                .kind = KeystrokeKind::Character,
                                                .typed = wrong},
                                      model_);
                }
            }

            [[nodiscard]] TypingModel& model() { return model_; }

        private:
            TypingModel model_;
        };

        [[nodiscard]] RaceParams params_with_lives(std::size_t lives) {
            RaceParams params;
            params.lives = lives;
            return params;
        }

        /// Past the opening grace, with the typist comfortably ahead.
        void get_going(RaceMode& mode, Typist& typist, Millis at = Millis{6'000}) {
            mode.on_start(Millis{0}, typist.model());
            typist.type(mode, 120, Millis{100});
            mode.on_tick(at, typist.model());
        }

        // ---- the opening grace ---------------------------------------------------

        TEST(RaceModeTest, TheFiveSecondStartGraceHoldsThePacerStill) {
            Typist typist;
            RaceMode mode{RaceParams{}, Wpm{60.0}};
            mode.on_start(Millis{0}, typist.model());

            mode.on_tick(Millis{4'900}, typist.model());

            EXPECT_EQ(mode.race().pacer, GraphemeIndex{0});
            EXPECT_FALSE(mode.is_finished()) << "and nobody is caught before the race begins";
        }

        TEST(RaceModeTest, NobodyIsCaughtDuringTheOpeningGrace) {
            // The typist has typed nothing and the lead is zero, which without
            // the grace is exactly the condition for being caught.
            Typist typist;
            RaceMode mode{RaceParams{}, Wpm{60.0}};
            mode.on_start(Millis{0}, typist.model());

            for (std::int64_t at = 100; at <= 4'900; at += 100) {
                mode.on_tick(Millis{at}, typist.model());
            }

            EXPECT_FALSE(mode.is_finished());
        }

        // ---- the grace window ------------------------------------------------------

        TEST(RaceModeTest, AMomentaryStumbleShorterThanTheGraceDoesNotEndTheRun) {
            // The whole reason the window exists: one fumbled keystroke must
            // not be fatal.
            Typist typist;
            RaceMode mode{RaceParams{}, Wpm{60.0}};
            get_going(mode, typist);

            // Let the pacer draw level and then overtake, but only briefly.
            mode.on_tick(Millis{30'000}, typist.model());
            ASSERT_LE(mode.race().lead, 0.0) << "the pacer is level or ahead";
            mode.on_tick(Millis{30'200}, typist.model());
            typist.type(mode, 200, Millis{30'250});
            mode.on_tick(Millis{30'300}, typist.model());

            EXPECT_FALSE(mode.is_finished()) << "two hundred milliseconds behind, and the grace is three hundred";
        }

        TEST(RaceModeTest, TheRunEndsOnceTheGraceHasFullyElapsed) {
            Typist typist;
            RaceMode mode{RaceParams{}, Wpm{60.0}};
            get_going(mode, typist);

            // Stop typing entirely; the pacer will pass and stay past.
            for (std::int64_t at = 30'000; at <= 60'000; at += 100) {
                mode.on_tick(Millis{at}, typist.model());
            }

            EXPECT_TRUE(mode.is_finished());
        }

        TEST(RaceModeTest, TheGraceIsMeasuredAtJustBelowAndJustAboveItsThreshold) {
            // The one duration that decides whether a fumble is survivable, so
            // it is asserted on both sides of itself rather than somewhere
            // comfortably either side.
            const RaceParams params;
            for (const std::int64_t behind_for: {params.grace.value - 1, params.grace.value}) {
                Typist typist;
                RaceMode mode{params, Wpm{60.0}};
                get_going(mode, typist);

                // Find the moment the lead first goes non-positive, then hold
                // exactly `behind_for` beyond it.
                std::int64_t at = 6'000;
                while (mode.race().lead > 0.0 && at < 120'000) {
                    at += 50;
                    mode.on_tick(Millis{at}, typist.model());
                }
                ASSERT_LE(mode.race().lead, 0.0) << "the pacer caught up at some point";
                ASSERT_FALSE(mode.is_finished()) << "but not instantly";

                mode.on_tick(Millis{at + behind_for}, typist.model());

                EXPECT_EQ(mode.is_finished(), behind_for >= params.grace.value)
                        << "behind for " << behind_for << " of " << params.grace.value;
            }
        }

        TEST(RaceModeTest, RegainingALeadResetsTheGraceTimer) {
            // Otherwise a race is lost by accumulating three hundred
            // milliseconds of stumbles over ten minutes, which is every race.
            Typist typist;
            RaceMode mode{RaceParams{}, Wpm{60.0}};
            get_going(mode, typist);

            for (std::int64_t round = 0; round < 20; ++round) {
                const std::int64_t at = 10'000 + round * 1'000;
                // Drift behind...
                mode.on_tick(Millis{at + 200}, typist.model());
                // ...then get back ahead before the grace runs out.
                typist.type(mode, 60, Millis{at + 250});
                mode.on_tick(Millis{at + 300}, typist.model());
                ASSERT_FALSE(mode.is_finished()) << "round " << round;
            }

            EXPECT_FALSE(mode.is_finished());
        }

        // ---- lives ---------------------------------------------------------------------

        TEST(RaceModeTest, BeingCaughtWithLivesLeftCostsOneAndContinues) {
            Typist typist;
            RaceMode mode{params_with_lives(3), Wpm{60.0}};
            get_going(mode, typist);
            ASSERT_EQ(mode.race().lives_left, 3U);

            for (std::int64_t at = 30'000; at <= 45'000; at += 100) {
                mode.on_tick(Millis{at}, typist.model());
            }

            EXPECT_LT(mode.race().lives_left, 3U) << "a life went";
            EXPECT_FALSE(mode.is_finished()) << "and the run did not";
        }

        TEST(RaceModeTest, BeingCaughtPushesThePacerBackAndTakesTheSpeedDown) {
            Typist typist;
            RaceMode mode{params_with_lives(3), Wpm{60.0}};
            get_going(mode, typist);
            const double before = mode.speed().value;

            for (std::int64_t at = 30'000; at <= 60'000; at += 100) {
                mode.on_tick(Millis{at}, typist.model());
                if (mode.race().lives_left < 3U) {
                    break;
                }
            }

            EXPECT_GT(mode.race().lead, 0.0) << "the ghost is behind the typist again";
            EXPECT_LT(mode.speed().value, before) << "and slower than it was";
        }

        TEST(RaceModeTest, ReachingZeroLivesEndsTheRun) {
            Typist typist;
            RaceMode mode{params_with_lives(2), Wpm{60.0}};
            get_going(mode, typist);

            for (std::int64_t at = 30'000; at <= 300'000; at += 100) {
                mode.on_tick(Millis{at}, typist.model());
            }

            EXPECT_TRUE(mode.is_finished());
            EXPECT_EQ(mode.race().lives_left, 0U);
        }

        TEST(RaceModeTest, OnceFinishedItStaysFinished) {
            // A mode that flickered back would restart a run the typist has
            // already been shown the results of.
            Typist typist;
            RaceMode mode{RaceParams{}, Wpm{60.0}};
            get_going(mode, typist);
            for (std::int64_t at = 30'000; at <= 60'000; at += 100) {
                mode.on_tick(Millis{at}, typist.model());
            }
            ASSERT_TRUE(mode.is_finished());

            typist.type(mode, 500, Millis{60'100});
            mode.on_tick(Millis{60'200}, typist.model());

            EXPECT_TRUE(mode.is_finished());
        }

        // ---- the accuracy the gate reads ---------------------------------------------

        TEST(RaceModeTest, RollingAccuracyIsOverTheWindowRatherThanTheWholeRun) {
            // A cumulative average stops responding to what the typist is doing
            // now, which is the same reason endless mode shows a rolling WPM.
            RaceParams params;
            params.accuracy_window = 20;
            Typist typist;
            RaceMode mode{params, Wpm{60.0}};
            mode.on_start(Millis{0}, typist.model());

            typist.type(mode, 200, Millis{100});
            ASSERT_DOUBLE_EQ(mode.race().accuracy.value, 1.0);
            typist.fumble(mode, 20, Millis{200});
            mode.on_tick(Millis{6'000}, typist.model());

            EXPECT_DOUBLE_EQ(mode.race().accuracy.value, 0.0) << "the window is entirely fumbles now";
        }

        TEST(RaceModeTest, NothingTypedYetIsNotNothingCorrectYet) {
            // Opening at zero accuracy would have the gate back the speed off
            // before the first key is pressed, which is a race that punishes
            // reading the first line.
            Typist typist;
            RaceMode mode{RaceParams{}, Wpm{60.0}};
            mode.on_start(Millis{0}, typist.model());

            mode.on_tick(Millis{100}, typist.model());

            EXPECT_DOUBLE_EQ(mode.race().accuracy.value, 1.0);
        }

        TEST(RaceModeTest, ABackspaceIsNotAnAttempt) {
            // Counting it would let the gate be moved by deleting rather than
            // by typing, which is the habit the mode exists to train out.
            RaceParams params;
            params.accuracy_window = 10;
            Typist typist;
            RaceMode mode{params, Wpm{60.0}};
            mode.on_start(Millis{0}, typist.model());
            typist.fumble(mode, 10, Millis{100});
            ASSERT_DOUBLE_EQ(mode.race().accuracy.value, 0.0);

            for (int undo = 0; undo < 10; ++undo) {
                typist.model().backspace(Millis{200});
                mode.on_keystroke(
                        Keystroke{
                                .at = Millis{200}, .target = 0, .kind = KeystrokeKind::Backspace, .typed = Grapheme{}},
                        typist.model());
            }

            EXPECT_DOUBLE_EQ(mode.race().accuracy.value, 0.0) << "deleting did not clean the window";
        }

        // ---- the sustained peak -----------------------------------------------------------

        TEST(RaceModeTest, ThePeakIsTheSpeedHeldRatherThanTheSpeedReached) {
            // Any lucky burst inflates the instantaneous peak, and it is this
            // number that becomes the next race's starting speed.
            RaceParams params;
            params.sustain_window = Millis{10'000};
            Typist typist;
            RaceMode mode{params, Wpm{60.0}};
            mode.on_start(Millis{0}, typist.model());

            // Climb briefly, then stop typing so the speed changes every tick.
            typist.type(mode, 400, Millis{100});
            mode.on_tick(Millis{6'000}, typist.model());
            mode.on_tick(Millis{7'000}, typist.model());

            EXPECT_DOUBLE_EQ(mode.race().peak_sustained.value, 0.0) << "nothing has been held for ten seconds yet";
        }

        // ---- endless ------------------------------------------------------------------------

        TEST(RaceModeTest, WithThePacerDisabledTheRunNeverEnds) {
            Typist typist;
            RaceMode mode{RaceParams{}, Wpm{60.0}, false};
            mode.on_start(Millis{0}, typist.model());

            for (std::int64_t at = 1'000; at <= 600'000; at += 1'000) {
                mode.on_tick(Millis{at}, typist.model());
            }

            EXPECT_FALSE(mode.is_finished());
            EXPECT_EQ(mode.id(), "endless");
        }

        TEST(RaceModeTest, APacedRunCallsItselfARace) { EXPECT_EQ(RaceMode(RaceParams{}, Wpm{60.0}).id(), "race"); }

        // ---- what a constant-rate typist actually gets ---------------------------------------

        TEST(RaceModeTest, ThePacerClimbsToMeetASteadyTypist) {
            // The half of TI-124's convergence property that holds: type
            // steadily and accurately and the ghost accelerates from well below
            // you up to your speed. It is what makes the mode responsive.
            constexpr double kPlayerWpm = 60.0;
            // 60 wpm is five graphemes a second, so one every 200 ms.
            constexpr std::int64_t kStepMs = 200;
            Typist typist;
            RaceMode mode{RaceParams{}, Wpm{30.0}};
            mode.on_start(Millis{0}, typist.model());

            for (std::int64_t at = kStepMs; at <= 60'000; at += kStepMs) {
                typist.type(mode, 1, Millis{at});
                mode.on_tick(Millis{at}, typist.model());
            }

            EXPECT_FALSE(mode.is_finished()) << "a minute in, a steady accurate typist is still going";
            EXPECT_GT(mode.speed().value, kPlayerWpm * 0.75)
                    << "and the ghost has climbed towards them, reaching " << mode.speed().value;
        }

        TEST(RaceModeTest, ThePacerOvershootsASteadyTypistAndEventuallyCatchesThem) {
            // **This is not the property TI-124 asks for, and it is what the
            // documented law does.** That issue wants a constant-rate typist to
            // converge near their own speed and be caught only after slowing
            // down; GAMEPLAY section 3.2 specifies a ramp that reads the lead
            // and not the rate the lead is changing at.
            //
            // Those two cannot both be true. The lead is the integral of the
            // speed difference, so by the time the ghost is faster than the
            // typist there is a large lead still to burn off — and the ramp
            // goes on climbing for the whole of it, because `f(lead)` is
            // saturated. Classic integrator windup: the overshoot is
            // structural, not a matter of constants, so no value of `k_up` or
            // `k_down` removes it.
            //
            // Asserted as measured rather than as hoped, so that a change to
            // the law shows up here as a decision somebody made. Resolving the
            // contradiction is TI-129's, and it needs a term the law does not
            // currently have.
            constexpr double kPlayerWpm = 60.0;
            constexpr std::int64_t kStepMs = 200;
            Typist typist;
            RaceMode mode{RaceParams{}, Wpm{30.0}};
            mode.on_start(Millis{0}, typist.model());

            double highest = 0.0;
            for (std::int64_t at = kStepMs; at <= 300'000; at += kStepMs) {
                typist.type(mode, 1, Millis{at});
                mode.on_tick(Millis{at}, typist.model());
                highest = std::max(highest, mode.speed().value);
                if (mode.is_finished()) {
                    break;
                }
            }

            EXPECT_GT(highest, kPlayerWpm * 1.20) << "it overshot to " << highest;
            EXPECT_TRUE(mode.is_finished()) << "and caught a typist who never slowed down";
        }

    }  // namespace
}  // namespace typeit::core
