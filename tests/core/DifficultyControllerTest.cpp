// The ramp law (TI-122).
//
// A difficulty ramp is exactly the kind of feature that is easy to get subtly
// and unfalsifiably wrong: it always produces *a* number, and a wrong one still
// looks like a race. So each case below asserts a property of the law rather
// than a value it happens to produce, and the properties are the design — if
// one of them stops holding, the mode has stopped doing the thing it exists for
// even if every run still completes.

#include <cstdint>
#include <gtest/gtest.h>
#include <vector>

#include "typeit/core/race/DifficultyController.h"
#include "typeit/core/race/RaceParams.h"
#include "typeit/core/util/Units.h"

namespace typeit::core {
    namespace {

        /// The Standard preset's numbers (GAMEPLAY section 3.5), which are the
        /// defaults.
        [[nodiscard]] RaceParams standard() { return RaceParams{}; }

        [[nodiscard]] DifficultyController at(double wpm, RaceParams params = standard()) {
            return DifficultyController{params, Wpm{wpm}};
        }

        constexpr Accuracy kPerfect{1.0};
        constexpr Millis kOneSecond{1'000};

        // ---- the accuracy gate ------------------------------------------------------

        TEST(DifficultyControllerTest, SpeedNeverRisesWhileAccuracyIsBelowTheGate) {
            // The single most important property in the phase. Without it the
            // mode rewards typing garbage quickly, which is the opposite of
            // what it exists to teach — so it is asserted across the whole
            // range of leads rather than at one convenient point.
            const RaceParams params = standard();
            for (const double lead: {0.0, 8.0, 24.0, 25.0, 60.0, 200.0, 10'000.0}) {
                for (const double accuracy: {0.0, 0.5, 0.90, 0.9199}) {
                    DifficultyController controller = at(60.0, params);

                    const Wpm after = controller.advance(kOneSecond, lead, Accuracy{accuracy});

                    EXPECT_LT(after.value, 60.0) << "lead " << lead << " at accuracy " << accuracy;
                }
            }
        }

        TEST(DifficultyControllerTest, PoorAccuracyBacksOffAtTheFullRateEvenWithAHugeLead) {
            DifficultyController controller = at(60.0);

            controller.advance(kOneSecond, 500.0, Accuracy{0.50});

            EXPECT_DOUBLE_EQ(controller.speed().value, 60.0 - standard().ramp_down);
            EXPECT_EQ(controller.trend(), RampTrend::BackingOff);
        }

        TEST(DifficultyControllerTest, TheGateIsProportionalAboveItRatherThanASwitch) {
            // Exactly at `A_min` the gain is zero and climbs from there, so
            // there is no cliff for a typist hovering on the boundary to fall
            // off.
            DifficultyController controller = at(60.0);

            EXPECT_DOUBLE_EQ(controller.rate(60.0, standard().min_accuracy), 0.0);
            EXPECT_GT(controller.rate(60.0, Accuracy{0.96}), 0.0);
            EXPECT_LT(controller.rate(60.0, Accuracy{0.96}), controller.rate(60.0, kPerfect));
        }

        // ---- the dead band -----------------------------------------------------------

        TEST(DifficultyControllerTest, SpeedIsExactlyUnchangedThroughoutTheDeadBand) {
            // Exactly, not nearly. This hysteresis is why the pacer does not
            // stutter; without it `V` oscillates every tick around the boundary
            // and the typist watches the ghost twitch.
            const RaceParams params = standard();
            for (double lead = params.lead_danger + 0.01; lead < params.lead_comfort; lead += 0.5) {
                DifficultyController controller = at(60.0, params);

                const Wpm after = controller.advance(kOneSecond, lead, kPerfect);

                EXPECT_DOUBLE_EQ(after.value, 60.0) << "lead " << lead;
                EXPECT_EQ(controller.trend(), RampTrend::Holding) << "lead " << lead;
            }
        }

        TEST(DifficultyControllerTest, TheBandIsClosedAtTheDangerEndAndOpenAtTheComfortEnd) {
            // At `lead_danger` exactly the speed falls, and at `lead_comfort`
            // exactly it is on the climbing branch — earning nothing, because
            // `f(lead)` is zero there, but no longer backing off.
            const RaceParams params = standard();
            DifficultyController controller = at(60.0, params);

            EXPECT_DOUBLE_EQ(controller.rate(params.lead_danger, kPerfect), -params.ramp_down);
            EXPECT_DOUBLE_EQ(controller.rate(params.lead_comfort, kPerfect), 0.0);
            EXPECT_GT(controller.rate(params.lead_comfort + 0.5, kPerfect), 0.0);
        }

        // ---- response to lead ----------------------------------------------------------

        TEST(DifficultyControllerTest, SpeedRisesMonotonicallyWithLeadAboveComfort) {
            DifficultyController controller = at(60.0);

            double previous = -1.0;
            for (double lead = standard().lead_comfort; lead <= 200.0; lead += 1.0) {
                const double rate = controller.rate(lead, kPerfect);
                EXPECT_GE(rate, previous) << "lead " << lead;
                previous = rate;
            }
        }

        TEST(DifficultyControllerTest, TheComfortFactorSaturatesAtOne) {
            // A typist two hundred graphemes ahead does not accelerate away
            // from a speed they were only briefly able to hold.
            const RaceParams params = standard();
            DifficultyController controller = at(60.0, params);
            const double saturated = params.lead_comfort + params.lead_scale;

            EXPECT_DOUBLE_EQ(controller.rate(saturated, kPerfect), params.ramp_up);
            EXPECT_DOUBLE_EQ(controller.rate(saturated * 4.0, kPerfect), params.ramp_up);
            EXPECT_DOUBLE_EQ(controller.rate(10'000.0, kPerfect), params.ramp_up);
        }

        TEST(DifficultyControllerTest, ANegativeLeadBacksOffRatherThanBeingArithmeticNobodyChecked) {
            // The typist is behind the pacer, which is an ordinary state for up
            // to `grace_ms` and has to have a defined answer.
            DifficultyController controller = at(60.0);

            EXPECT_DOUBLE_EQ(controller.rate(-40.0, kPerfect), -standard().ramp_down);
        }

        // ---- the clamps -------------------------------------------------------------

        TEST(DifficultyControllerTest, TheSpeedIsClampedInBothDirections) {
            const RaceParams params = standard();

            DifficultyController falling = at(params.min_speed.value + 0.5, params);
            for (int step = 0; step < 100; ++step) {
                falling.advance(kOneSecond, 0.0, kPerfect);
            }
            EXPECT_DOUBLE_EQ(falling.speed().value, params.min_speed.value);

            DifficultyController rising = at(params.max_speed.value - 0.5, params);
            for (int step = 0; step < 100; ++step) {
                rising.advance(kOneSecond, 500.0, kPerfect);
            }
            EXPECT_DOUBLE_EQ(rising.speed().value, params.max_speed.value);
        }

        TEST(DifficultyControllerTest, AStartOutsideTheBandIsBroughtIntoIt) {
            EXPECT_DOUBLE_EQ(at(1.0).speed().value, standard().min_speed.value);
            EXPECT_DOUBLE_EQ(at(9'000.0).speed().value, standard().max_speed.value);
        }

        // ---- time ---------------------------------------------------------------------

        TEST(DifficultyControllerTest, TwoHalfStepsEqualOneWholeStep) {
            // The law is linear in Δt, so this is exact rather than merely
            // close — and it is what makes the ramp independent of the frame
            // rate it happens to be ticked at.
            DifficultyController whole = at(60.0);
            DifficultyController halves = at(60.0);

            whole.advance(Millis{1'000}, 60.0, kPerfect);
            halves.advance(Millis{500}, 60.0, kPerfect);
            halves.advance(Millis{500}, 60.0, kPerfect);

            EXPECT_NEAR(whole.speed().value, halves.speed().value, 1e-12);
        }

        TEST(DifficultyControllerTest, AThousandTinyStepsMatchOneLongOne) {
            DifficultyController whole = at(60.0);
            DifficultyController stepped = at(60.0);

            whole.advance(Millis{10'000}, 60.0, kPerfect);
            for (int step = 0; step < 1'000; ++step) {
                stepped.advance(Millis{10}, 60.0, kPerfect);
            }

            EXPECT_NEAR(whole.speed().value, stepped.speed().value, 1e-9);
        }

        TEST(DifficultyControllerTest, TimeThatDidNotPassChangesNothingButStillReportsWhy) {
            DifficultyController controller = at(60.0);

            controller.advance(Millis{0}, 0.0, kPerfect);

            EXPECT_DOUBLE_EQ(controller.speed().value, 60.0);
            EXPECT_EQ(controller.trend(), RampTrend::BackingOff) << "the HUD should still say why nothing is gained";
        }

        // ---- recovery -------------------------------------------------------------------

        TEST(DifficultyControllerTest, AStumbleIsRecoveredFromRatherThanSpiralling) {
            // `k_down > k_up` means a stumble costs more than it earns back,
            // which is deliberate — but it must buy breathing room rather than
            // start a slide nobody climbs out of. Scripted rather than argued.
            DifficultyController controller = at(60.0);

            for (int second = 0; second < 5; ++second) {
                controller.advance(kOneSecond, 4.0, Accuracy{0.70});
            }
            const double bottom = controller.speed().value;
            ASSERT_LT(bottom, 60.0) << "the stumble cost something";

            for (int second = 0; second < 30; ++second) {
                controller.advance(kOneSecond, 60.0, kPerfect);
            }

            EXPECT_GT(controller.speed().value, bottom) << "and it climbs again";
            EXPECT_GT(controller.speed().value, 60.0) << "past where it was, given enough good typing";
        }

        // ---- being caught ------------------------------------------------------------------

        TEST(DifficultyControllerTest, BeingCaughtCostsAFractionOfTheSpeed) {
            DifficultyController controller = at(100.0);

            controller.apply_catch_penalty();

            EXPECT_DOUBLE_EQ(controller.speed().value, 90.0);
        }

        TEST(DifficultyControllerTest, TheCatchPenaltyCannotPushBelowTheFloor) {
            DifficultyController controller = at(standard().min_speed.value);

            controller.apply_catch_penalty();

            EXPECT_DOUBLE_EQ(controller.speed().value, standard().min_speed.value);
        }

        // ---- parameters that would divide by zero -----------------------------------------

        TEST(DifficultyControllerTest, AZeroLeadScaleMeansAnyLeadIsFullComfort) {
            // Rather than a NaN propagating into the speed, then into the
            // pacer, and from there into every decision the run makes.
            RaceParams params = standard();
            params.lead_scale = 0.0;
            DifficultyController controller = at(60.0, params);

            EXPECT_DOUBLE_EQ(controller.rate(params.lead_comfort, kPerfect), params.ramp_up);
        }

        TEST(DifficultyControllerTest, AGateOfOneEarnsOnlyOnPerfectAccuracy) {
            RaceParams params = standard();
            params.min_accuracy = Accuracy{1.0};
            DifficultyController controller = at(60.0, params);

            EXPECT_DOUBLE_EQ(controller.rate(200.0, kPerfect), params.ramp_up);
            EXPECT_DOUBLE_EQ(controller.rate(200.0, Accuracy{0.999}), -params.ramp_down);
        }

        // ---- the shipped presets -------------------------------------------------------------

        TEST(DifficultyControllerTest, BackingOffIsFasterThanClimbingInEveryShippedPreset) {
            // Asserted on the parameters rather than on behaviour, so a future
            // edit to the table cannot quietly turn a stumble into a spiral.
            // The preset table itself arrives with TI-123; this is the default
            // set, which is Standard.
            const RaceParams params = standard();

            EXPECT_GT(params.ramp_down, params.ramp_up);
        }

        TEST(DifficultyControllerTest, TheDefaultsAreTheStandardPresetFromTheDocument) {
            // A default that is not one of the three presets would be a fourth
            // difficulty nobody chose.
            const RaceParams params = standard();

            EXPECT_DOUBLE_EQ(params.ramp_up, 0.60);
            EXPECT_DOUBLE_EQ(params.ramp_down, 1.50);
            EXPECT_DOUBLE_EQ(params.min_accuracy.value, 0.92);
            EXPECT_DOUBLE_EQ(params.lead_comfort, 25.0);
            EXPECT_DOUBLE_EQ(params.lead_danger, 8.0);
            EXPECT_DOUBLE_EQ(params.lead_scale, 30.0);
            EXPECT_EQ(params.grace, Millis{300});
            EXPECT_EQ(params.lives, 1U);
            EXPECT_DOUBLE_EQ(params.start_factor, 0.85);
        }

        // ---- the whole curve -----------------------------------------------------------------

        TEST(DifficultyControllerTest, AScriptedRunMatchesItsGoldenSpeedCurve) {
            // One number per second of a run that climbs, stumbles, recovers
            // and climbs again. Committed rather than computed, so that a
            // change to any constant shows up as a diff somebody has to read
            // and agree with — which is what TI-129's tuning pass regenerates.
            struct Step {
                double lead;
                double accuracy;
            };
            const std::vector<Step> script{
                    // Ten seconds comfortably ahead and accurate: climbing.
                    {60.0, 1.00},
                    {60.0, 1.00},
                    {60.0, 1.00},
                    {60.0, 1.00},
                    {60.0, 1.00},
                    {60.0, 1.00},
                    {60.0, 1.00},
                    {60.0, 1.00},
                    {60.0, 1.00},
                    {60.0, 1.00},
                    // Four seconds in the dead band: unchanged.
                    {15.0, 1.00},
                    {15.0, 1.00},
                    {15.0, 1.00},
                    {15.0, 1.00},
                    // Three seconds of a stumble: accuracy gone, backing off.
                    {30.0, 0.60},
                    {6.0, 0.60},
                    {6.0, 0.70},
                    // And six recovering.
                    {40.0, 0.98},
                    {50.0, 0.98},
                    {60.0, 0.99},
                    {60.0, 1.00},
                    {60.0, 1.00},
                    {60.0, 1.00},
            };
            const std::vector<double> golden{
                    // Ten seconds at the full +0.60/s: 60 to 66.
                    60.600,
                    61.200,
                    61.800,
                    62.400,
                    63.000,
                    63.600,
                    64.200,
                    64.800,
                    65.400,
                    66.000,
                    // Four in the dead band, unchanged.
                    66.000,
                    66.000,
                    66.000,
                    66.000,
                    // Three of stumble at -1.50/s, the first of them gated by
                    // accuracy alone despite a comfortable lead.
                    64.500,
                    63.000,
                    61.500,
                    // Then recovery, accelerating as the lead opens back up.
                    61.725,
                    62.100,
                    62.625,
                    63.225,
                    63.825,
                    64.425,
            };
            ASSERT_EQ(script.size(), golden.size());
            DifficultyController controller = at(60.0);

            for (std::size_t step = 0; step < script.size(); ++step) {
                const Wpm after = controller.advance(kOneSecond, script[step].lead, Accuracy{script[step].accuracy});

                EXPECT_NEAR(after.value, golden[step], 1e-9) << "at second " << step + 1;
            }
        }

    }  // namespace
}  // namespace typeit::core
