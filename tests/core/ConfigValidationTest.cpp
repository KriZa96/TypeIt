#include <cstdint>
#include <functional>
#include <gtest/gtest.h>
#include <limits>
#include <string>
#include <string_view>
#include <vector>

#include "typeit/core/config/Config.h"
#include "typeit/core/config/Validation.h"
#include "typeit/core/util/Result.h"

namespace typeit::core {
    namespace {

        TEST(ConfigValidationTest, TheDefaultsAreValid) {
            const Config defaults;

            const Status status = validate(defaults);

            EXPECT_TRUE(status) << (status ? "" : status.error().context);
        }

        TEST(ConfigValidationTest, LineWidthZeroIsValidAndMeansFitTheTerminal) {
            Config config;
            config.appearance.line_width = 0;

            EXPECT_TRUE(validate(config));
        }

        TEST(ConfigValidationTest, ValidationIsPure) {
            // Same input, same answer, nothing touched. The property that lets
            // the same rule serve a config file, a CLI override and a settings
            // screen.
            Config config;
            config.general.default_duration_s = 0;
            const Config before = config;

            const Status first = validate(config);
            const Status second = validate(config);

            ASSERT_FALSE(first);
            ASSERT_FALSE(second);
            EXPECT_EQ(first.error().context, second.error().context);
            EXPECT_EQ(config.general.default_duration_s, before.general.default_duration_s);
        }

        // Every numeric field, on both sides of its documented range.
        struct NumericCase {
            std::string_view field;
            std::function<void(Config&)> set_too_low;
            std::function<void(Config&)> set_too_high;
            std::function<void(Config&)> set_valid;
        };

        class NumericRangeTest : public ::testing::TestWithParam<NumericCase> {};

        TEST_P(NumericRangeTest, RejectsOutsideTheRangeAndAcceptsInside) {
            const NumericCase& field = GetParam();

            Config too_low;
            field.set_too_low(too_low);
            const Status low = validate(too_low);
            ASSERT_FALSE(low) << field.field << " accepted a value below its range";
            EXPECT_EQ(low.error().code, ErrorCode::ConfigInvalid);
            EXPECT_TRUE(low.error().context.starts_with(field.field)) << low.error().context;

            Config too_high;
            field.set_too_high(too_high);
            const Status high = validate(too_high);
            ASSERT_FALSE(high) << field.field << " accepted a value above its range";
            EXPECT_TRUE(high.error().context.starts_with(field.field)) << high.error().context;

            Config valid;
            field.set_valid(valid);
            EXPECT_TRUE(validate(valid)) << field.field << " rejected a value inside its range";
        }

        std::vector<NumericCase> numeric_fields() {
            return {
                    {"config_version", [](Config& c) { c.config_version = 0; },
                     [](Config& c) { c.config_version = 1'001; }, [](Config& c) { c.config_version = 1; }},
                    {"general.default_duration_s", [](Config& c) { c.general.default_duration_s = 0; },
                     [](Config& c) { c.general.default_duration_s = 3'601; },
                     [](Config& c) { c.general.default_duration_s = 3'600; }},
                    {"general.default_word_count", [](Config& c) { c.general.default_word_count = 0; },
                     [](Config& c) { c.general.default_word_count = 10'001; },
                     [](Config& c) { c.general.default_word_count = 1; }},
                    {"general.countdown_s", [](Config& c) { c.general.countdown_s = -1; },
                     [](Config& c) { c.general.countdown_s = 6; }, [](Config& c) { c.general.countdown_s = 0; }},
                    {"appearance.line_width", [](Config& c) { c.appearance.line_width = -1; },
                     [](Config& c) { c.appearance.line_width = 501; }, [](Config& c) { c.appearance.line_width = 80; }},
                    {"appearance.lines_visible", [](Config& c) { c.appearance.lines_visible = 0; },
                     [](Config& c) { c.appearance.lines_visible = 51; },
                     [](Config& c) { c.appearance.lines_visible = 3; }},
                    {"text.tab_width", [](Config& c) { c.text.tab_width = 0; },
                     [](Config& c) { c.text.tab_width = 17; }, [](Config& c) { c.text.tab_width = 8; }},
                    {"text.chunk_graphemes", [](Config& c) { c.text.chunk_graphemes = 49; },
                     [](Config& c) { c.text.chunk_graphemes = 100'001; },
                     [](Config& c) { c.text.chunk_graphemes = 50; }},
                    {"race.start_wpm", [](Config& c) { c.race.start_wpm = 0; },
                     [](Config& c) { c.race.start_wpm = 501; }, [](Config& c) { c.race.start_wpm = 40; }},
                    {"race.ramp_up", [](Config& c) { c.race.ramp_up = 0.0; }, [](Config& c) { c.race.ramp_up = 101.0; },
                     [](Config& c) { c.race.ramp_up = 1.0; }},
                    {"race.ramp_down", [](Config& c) { c.race.ramp_down = -0.1; },
                     [](Config& c) { c.race.ramp_down = 101.0; }, [](Config& c) { c.race.ramp_down = 0.0; }},
                    {"race.min_accuracy", [](Config& c) { c.race.min_accuracy = 0.0; },
                     [](Config& c) { c.race.min_accuracy = 1.0; }, [](Config& c) { c.race.min_accuracy = 0.5; }},
                    {"race.lead_comfort", [](Config& c) { c.race.lead_comfort = 0; },
                     [](Config& c) { c.race.lead_comfort = 10'001; }, [](Config& c) { c.race.lead_comfort = 30; }},
                    {"race.lead_danger", [](Config& c) { c.race.lead_danger = 0; },
                     [](Config& c) { c.race.lead_danger = 10'001; }, [](Config& c) { c.race.lead_danger = 5; }},
                    {"race.lead_scale", [](Config& c) { c.race.lead_scale = 0; },
                     [](Config& c) { c.race.lead_scale = 10'001; }, [](Config& c) { c.race.lead_scale = 20; }},
                    {"race.grace_ms", [](Config& c) { c.race.grace_ms = -1; },
                     [](Config& c) { c.race.grace_ms = 10'001; }, [](Config& c) { c.race.grace_ms = 0; }},
                    {"race.lives", [](Config& c) { c.race.lives = 0; }, [](Config& c) { c.race.lives = 101; },
                     [](Config& c) { c.race.lives = 3; }},
                    {"race.sustain_window_s", [](Config& c) { c.race.sustain_window_s = 0; },
                     [](Config& c) { c.race.sustain_window_s = 3'601; },
                     [](Config& c) { c.race.sustain_window_s = 15; }},
                    {"history.retention_days", [](Config& c) { c.history.retention_days = -1; },
                     [](Config& c) { c.history.retention_days = 36'501; },
                     [](Config& c) { c.history.retention_days = 0; }},
                    {"network.timeout_s", [](Config& c) { c.network.timeout_s = 0; },
                     [](Config& c) { c.network.timeout_s = 601; }, [](Config& c) { c.network.timeout_s = 30; }},
                    {"network.max_size_mb", [](Config& c) { c.network.max_size_mb = 0; },
                     [](Config& c) { c.network.max_size_mb = 1'001; }, [](Config& c) { c.network.max_size_mb = 10; }},
            };
        }

        INSTANTIATE_TEST_SUITE_P(AllNumericFields, NumericRangeTest, ::testing::ValuesIn(numeric_fields()),
                                 [](const ::testing::TestParamInfo<NumericCase>& parameter) {
                                     std::string name{parameter.param.field};
                                     for (char& character: name) {
                                         if (character == '.') {
                                             character = '_';
                                         }
                                     }
                                     return name;
                                 });

        // Every enum-like string field, with a value that is not one of them.
        struct EnumCase {
            std::string_view field;
            std::function<void(Config&)> set_unknown;
            std::string_view expected_member;
        };

        class EnumFieldTest : public ::testing::TestWithParam<EnumCase> {};

        TEST_P(EnumFieldTest, RejectsAnUnknownValueAndNamesTheValidSet) {
            const EnumCase& field = GetParam();
            Config config;
            field.set_unknown(config);

            const Status status = validate(config);

            ASSERT_FALSE(status) << field.field << " accepted a value that is not one of its own";
            EXPECT_EQ(status.error().code, ErrorCode::ConfigInvalid);
            EXPECT_TRUE(status.error().context.starts_with(field.field)) << status.error().context;
            EXPECT_NE(status.error().context.find("expected one of:"), std::string::npos) << status.error().context;
            EXPECT_NE(status.error().context.find(field.expected_member), std::string::npos)
                    << "the message has to say what would have worked: " << status.error().context;
        }

        std::vector<EnumCase> enum_fields() {
            return {
                    {"general.default_mode", [](Config& c) { c.general.default_mode = "tiemd"; }, "quote"},
                    {"general.log_level", [](Config& c) { c.general.log_level = "verbose"; }, "debug"},
                    {"appearance.color_depth", [](Config& c) { c.appearance.color_depth = "24bit"; }, "truecolor"},
                    {"appearance.glyphs", [](Config& c) { c.appearance.glyphs = "emoji"; }, "unicode"},
                    {"appearance.caret", [](Config& c) { c.appearance.caret = "bar"; }, "underline"},
                    {"appearance.layout", [](Config& c) { c.appearance.layout = "cosy"; }, "comfortable"},
                    {"race.preset", [](Config& c) { c.race.preset = "medium"; }, "standard"},
                    {"race.start_policy", [](Config& c) { c.race.start_policy = "average"; }, "from_history"},
            };
        }

        INSTANTIATE_TEST_SUITE_P(AllEnumFields, EnumFieldTest, ::testing::ValuesIn(enum_fields()),
                                 [](const ::testing::TestParamInfo<EnumCase>& parameter) {
                                     std::string name{parameter.param.field};
                                     for (char& character: name) {
                                         if (character == '.') {
                                             character = '_';
                                         }
                                     }
                                     return name;
                                 });

        // The race cross-checks, which no single field's range can catch.

        TEST(ConfigValidationTest, DangerMustBeNearerThanComfort) {
            Config config;
            config.race.lead_comfort = 10;
            config.race.lead_danger = 10;

            const Status status = validate(config);

            ASSERT_FALSE(status);
            EXPECT_TRUE(status.error().context.starts_with("race.lead_danger")) << status.error().context;
            EXPECT_NE(status.error().context.find("lead_comfort"), std::string::npos)
                    << "a cross-field error names both fields: " << status.error().context;
        }

        TEST(ConfigValidationTest, DangerBelowComfortIsFine) {
            Config config;
            config.race.lead_comfort = 10;
            config.race.lead_danger = 9;

            EXPECT_TRUE(validate(config));
        }

        TEST(ConfigValidationTest, MinAccuracyIsAnOpenInterval) {
            // A race that demands perfection is unplayable; one that demands
            // nothing is not a race.
            Config perfect;
            perfect.race.min_accuracy = 1.0;
            EXPECT_FALSE(validate(perfect));

            Config nothing;
            nothing.race.min_accuracy = 0.0;
            EXPECT_FALSE(validate(nothing));

            Config sensible;
            sensible.race.min_accuracy = 0.92;
            EXPECT_TRUE(validate(sensible));
        }

        TEST(ConfigValidationTest, ANotANumberIsRejectedRatherThanCompared) {
            // NaN compares false against every bound, so a naive `value < low ||
            // value > high` lets it straight through.
            Config config;
            config.race.min_accuracy = std::numeric_limits<double>::quiet_NaN();

            EXPECT_FALSE(validate(config));
        }

        TEST(ConfigValidationTest, TheFirstProblemIsTheOneReported) {
            // A person edits a config file one mistake at a time; a wall of
            // complaints about a file with one typo is worse than a sentence.
            Config config;
            config.general.default_duration_s = 0;
            config.race.lives = 0;

            const Status status = validate(config);

            ASSERT_FALSE(status);
            EXPECT_TRUE(status.error().context.starts_with("general.default_duration_s")) << status.error().context;
        }

        TEST(ConfigValidationTest, TypingRulesCarryTheirOwnDefaults) {
            // They are validated by their type: an enum cannot hold a value
            // that is not one of its own, which is why [typing] has no string
            // fields here.
            const Config config;

            EXPECT_EQ(config.typing, TypingRules{});
            EXPECT_TRUE(validate(config));
        }

        TEST(ConfigValidationTest, OpaqueMapsArePassedThroughUntouched) {
            // Whether a key spelling is deliverable is the keymap parser's
            // question, and whether a converter command exists is infra's.
            Config config;
            config.keys["quit"] = "ctrl-q";
            config.import_.converters["application/pdf"] = "pdftotext {input} -";

            EXPECT_TRUE(validate(config));
        }

    }  // namespace
}  // namespace typeit::core
