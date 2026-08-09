// The three difficulties (TI-123).
//
// Two kinds of case here. The first is transcription: each preset holds exactly
// the values in GAMEPLAY section 3.5, so the table cannot drift from the
// document that explains it. The second is the *ordering* between them, which
// matters more — "Gentle" that ramps faster than "Brutal" is a bug nobody would
// ever report, because both still produce a perfectly good race.

#include <gtest/gtest.h>
#include <optional>
#include <string>

#include "typeit/core/config/Config.h"
#include "typeit/core/race/RaceParams.h"
#include "typeit/core/race/RacePresets.h"
#include "typeit/core/util/Result.h"
#include "typeit/core/util/Units.h"

namespace typeit::core {
    namespace {

        [[nodiscard]] RaceConfig custom() {
            RaceConfig config;
            config.preset = "custom";
            return config;
        }

        // ---- the table, against the document ---------------------------------------

        TEST(RaceParamsTest, GentleIsTheDocumentsGentleColumn) {
            const RaceParams params = preset_params(RacePreset::Gentle);

            EXPECT_DOUBLE_EQ(params.ramp_up, 0.35);
            EXPECT_DOUBLE_EQ(params.ramp_down, 2.00);
            EXPECT_DOUBLE_EQ(params.min_accuracy.value, 0.88);
            EXPECT_DOUBLE_EQ(params.lead_comfort, 35.0);
            EXPECT_DOUBLE_EQ(params.lead_danger, 12.0);
            EXPECT_DOUBLE_EQ(params.lead_scale, 40.0);
            EXPECT_EQ(params.grace, Millis{600});
            EXPECT_EQ(params.lives, 3U);
            EXPECT_DOUBLE_EQ(params.start_factor, 0.75);
        }

        TEST(RaceParamsTest, StandardIsTheDocumentsStandardColumn) {
            const RaceParams params = preset_params(RacePreset::Standard);

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

        TEST(RaceParamsTest, BrutalIsTheDocumentsBrutalColumn) {
            const RaceParams params = preset_params(RacePreset::Brutal);

            EXPECT_DOUBLE_EQ(params.ramp_up, 1.00);
            EXPECT_DOUBLE_EQ(params.ramp_down, 1.00);
            EXPECT_DOUBLE_EQ(params.min_accuracy.value, 0.95);
            EXPECT_DOUBLE_EQ(params.lead_comfort, 15.0);
            EXPECT_DOUBLE_EQ(params.lead_danger, 4.0);
            EXPECT_DOUBLE_EQ(params.lead_scale, 20.0);
            EXPECT_EQ(params.grace, Millis{100});
            EXPECT_EQ(params.lives, 1U);
            EXPECT_DOUBLE_EQ(params.start_factor, 0.95);
        }

        TEST(RaceParamsTest, TheDefaultsAreStandardRatherThanAFourthDifficulty) {
            const RaceParams defaults;

            EXPECT_DOUBLE_EQ(defaults.ramp_up, preset_params(RacePreset::Standard).ramp_up);
            EXPECT_DOUBLE_EQ(defaults.min_accuracy.value, preset_params(RacePreset::Standard).min_accuracy.value);
            EXPECT_EQ(defaults.grace, preset_params(RacePreset::Standard).grace);
        }

        // ---- the ordering ------------------------------------------------------------

        TEST(RaceParamsTest, EachPresetIsHarderThanTheOneBeforeIt) {
            // Asserted on the parameters rather than on play, so an edit to the
            // table cannot silently invert the difficulty ordering — which is a
            // bug nobody reports, because a wrongly-ordered Gentle is still a
            // working race.
            const RaceParams gentle = preset_params(RacePreset::Gentle);
            const RaceParams standard = preset_params(RacePreset::Standard);
            const RaceParams brutal = preset_params(RacePreset::Brutal);

            EXPECT_LT(gentle.ramp_up, standard.ramp_up) << "gentle climbs more slowly";
            EXPECT_LT(standard.ramp_up, brutal.ramp_up);

            EXPECT_GT(gentle.ramp_down, standard.ramp_down) << "and backs off faster, which is breathing room";
            EXPECT_GT(standard.ramp_down, brutal.ramp_down);

            EXPECT_LT(gentle.min_accuracy, standard.min_accuracy) << "and demands less accuracy";
            EXPECT_LT(standard.min_accuracy, brutal.min_accuracy);

            EXPECT_GT(gentle.grace, standard.grace) << "and forgives being caught for longer";
            EXPECT_GT(standard.grace, brutal.grace);

            EXPECT_GE(gentle.lives, standard.lives) << "and gives at least as many lives";
            EXPECT_GE(standard.lives, brutal.lives);

            EXPECT_LT(gentle.start_factor, standard.start_factor) << "and starts further below your best";
            EXPECT_LT(standard.start_factor, brutal.start_factor);

            EXPECT_GT(gentle.lead_comfort, standard.lead_comfort) << "and wants more lead before it climbs";
            EXPECT_GT(standard.lead_comfort, brutal.lead_comfort);
        }

        TEST(RaceParamsTest, EveryPresetBacksOffAtLeastAsFastAsItClimbs) {
            // `k_down >= k_up` is what makes a stumble breathing room rather
            // than the start of a slide. Brutal is the equality case, on
            // purpose.
            for (const RacePreset preset: race_presets()) {
                const RaceParams params = preset_params(preset);

                EXPECT_GE(params.ramp_down, params.ramp_up) << to_string(preset);
            }
        }

        TEST(RaceParamsTest, EveryPresetIsInternallyValid) {
            for (const RacePreset preset: race_presets()) {
                const Status usable = validate(preset_params(preset));

                EXPECT_TRUE(usable) << to_string(preset) << ": " << (usable ? "" : usable.error().context);
            }
        }

        // ---- names --------------------------------------------------------------------

        TEST(RaceParamsTest, TheNamesRoundTrip) {
            for (const RacePreset preset: race_presets()) {
                const std::optional<RacePreset> parsed = race_preset_from(to_string(preset));

                ASSERT_TRUE(parsed.has_value()) << to_string(preset);
                EXPECT_EQ(*parsed, preset);
            }
        }

        TEST(RaceParamsTest, CustomIsNotAPreset) {
            // It is a real configuration value and the absence of a difficulty,
            // so the two questions stay apart.
            EXPECT_FALSE(race_preset_from("custom").has_value());
            EXPECT_FALSE(race_preset_from("").has_value());
            EXPECT_FALSE(race_preset_from("Standard").has_value()) << "the config vocabulary is lower case";
        }

        // ---- resolving a config ----------------------------------------------------------

        TEST(RaceParamsTest, ANamedPresetUsesTheTableAndIgnoresTheExplicitValues) {
            // Otherwise a stale `ramp_up` left in a file from an afternoon of
            // experimenting follows somebody into every preset they choose
            // afterwards, and "standard" stops meaning the same thing in two
            // config files.
            RaceConfig config;
            config.preset = "brutal";
            config.ramp_up = 99.0;

            const Result<RaceParams> params = race_params_from(config);

            ASSERT_TRUE(params) << (params ? "" : params.error().context);
            EXPECT_DOUBLE_EQ(params->ramp_up, 1.00);
        }

        TEST(RaceParamsTest, CustomUsesTheExplicitValues) {
            RaceConfig config = custom();
            config.ramp_up = 0.42;
            config.lead_comfort = 40;
            config.lead_danger = 9;
            config.lives = 5;

            const Result<RaceParams> params = race_params_from(config);

            ASSERT_TRUE(params) << (params ? "" : params.error().context);
            EXPECT_DOUBLE_EQ(params->ramp_up, 0.42);
            EXPECT_DOUBLE_EQ(params->lead_comfort, 40.0);
            EXPECT_EQ(params->lives, 5U);
        }

        TEST(RaceParamsTest, ThePresetIsNamedWhenItIsNotOne) {
            // Rather than silently falling back to Standard: a typo in a
            // difficulty is somebody playing the wrong game and wondering why.
            RaceConfig config;
            config.preset = "gentel";

            const Result<RaceParams> params = race_params_from(config);

            ASSERT_FALSE(params);
            EXPECT_EQ(params.error().code, ErrorCode::ConfigInvalid);
            EXPECT_NE(params.error().context.find("gentel"), std::string::npos) << params.error().context;
        }

        // ---- the relationships ------------------------------------------------------------

        TEST(RaceParamsTest, TheDangerZoneHasToBeBelowTheComfortZone) {
            RaceConfig config = custom();
            config.lead_danger = 30;
            config.lead_comfort = 25;

            const Result<RaceParams> params = race_params_from(config);

            ASSERT_FALSE(params);
            EXPECT_NE(params.error().context.find("lead_comfort"), std::string::npos) << params.error().context;
        }

        TEST(RaceParamsTest, TheAccuracyGateIsOpenAtBothEnds) {
            // A race that demands perfection is unplayable; one that demands
            // nothing is not teaching anybody to type.
            for (const double gate: {0.0, 1.0}) {
                RaceParams params;
                params.min_accuracy = Accuracy{gate};

                EXPECT_FALSE(validate(params)) << gate;
            }
        }

        TEST(RaceParamsTest, ARaceThatCannotSpeedUpIsRefused) {
            // It would be a timed run with a ghost in it.
            RaceParams params;
            params.ramp_up = 0.0;

            const Status usable = validate(params);

            ASSERT_FALSE(usable);
            EXPECT_NE(usable.error().context.find("ramp_up"), std::string::npos) << usable.error().context;
        }

        TEST(RaceParamsTest, ZeroLivesIsARunThatEndsBeforeItStarts) {
            RaceParams params;
            params.lives = 0;

            EXPECT_FALSE(validate(params));
        }

        TEST(RaceParamsTest, TheSpeedBandHasToHaveWidth) {
            RaceParams params;
            params.min_speed = Wpm{100.0};
            params.max_speed = Wpm{50.0};

            const Status usable = validate(params);

            ASSERT_FALSE(usable);
            EXPECT_NE(usable.error().context.find("max_speed"), std::string::npos) << usable.error().context;
        }

        TEST(RaceParamsTest, ACatchPenaltyOutsideItsRangeIsRefused) {
            // One sets the speed to zero and the race stops being one; a
            // negative rewards being caught.
            for (const double penalty: {-0.1, 1.0, 2.0}) {
                RaceParams params;
                params.catch_penalty = penalty;

                EXPECT_FALSE(validate(params)) << penalty;
            }
        }

        TEST(RaceParamsTest, AStartFactorAboveOneWouldUndoTheProgression) {
            // It would start the next race faster than anything the typist has
            // ever held, which is the opposite of what the factor is for.
            RaceParams params;
            params.start_factor = 1.5;

            EXPECT_FALSE(validate(params));
            params.start_factor = 1.0;
            EXPECT_TRUE(validate(params)) << "exactly one is 'start where you left off', which is allowed";
        }

    }  // namespace
}  // namespace typeit::core
