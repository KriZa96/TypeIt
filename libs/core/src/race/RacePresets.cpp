#include "typeit/core/race/RacePresets.h"

#include <array>
#include <cstddef>
#include <optional>
#include <span>
#include <string>
#include <string_view>

#include "typeit/core/config/Config.h"
#include "typeit/core/race/RaceParams.h"
#include "typeit/core/util/Result.h"
#include "typeit/core/util/Units.h"

namespace typeit::core {
    namespace {

        /// GAMEPLAY section 3.5, transcribed once.
        ///
        /// The values that are not in the document's table — the speed band,
        /// the catch penalty, the accuracy window — are the same across all
        /// three on purpose: they are limits and mechanics rather than
        /// difficulty, and varying them would be a fourth axis nobody asked
        /// for.
        [[nodiscard]] RaceParams gentle() {
            RaceParams params;
            params.ramp_up = 0.35;
            params.ramp_down = 2.00;
            params.min_accuracy = Accuracy{0.88};
            params.lead_comfort = 35.0;
            params.lead_danger = 12.0;
            params.lead_scale = 40.0;
            params.grace = Millis{600};
            params.lives = 3;
            params.start_factor = 0.75;
            return params;
        }

        [[nodiscard]] RaceParams brutal() {
            RaceParams params;
            params.ramp_up = 1.00;
            params.ramp_down = 1.00;
            params.min_accuracy = Accuracy{0.95};
            params.lead_comfort = 15.0;
            params.lead_danger = 4.0;
            params.lead_scale = 20.0;
            params.grace = Millis{100};
            params.lives = 1;
            params.start_factor = 0.95;
            return params;
        }

        [[nodiscard]] std::string number(double value) {
            std::string text = std::to_string(value);
            // Trailing zeros make `0.920000` out of `0.92`, which reads as
            // precision nobody claimed.
            while (text.size() > 1 && text.back() == '0') {
                text.pop_back();
            }
            if (!text.empty() && text.back() == '.') {
                text.pop_back();
            }
            return text;
        }

    }  // namespace

    std::string_view to_string(RacePreset preset) {
        switch (preset) {
            case RacePreset::Gentle:
                return "gentle";
            case RacePreset::Standard:
                return "standard";
            case RacePreset::Brutal:
                return "brutal";
        }
        return "standard";
    }

    std::span<const RacePreset> race_presets() {
        static constexpr std::array<RacePreset, 3> kAll{RacePreset::Gentle, RacePreset::Standard, RacePreset::Brutal};
        return kAll;
    }

    std::optional<RacePreset> race_preset_from(std::string_view name) {
        for (const RacePreset preset: race_presets()) {
            if (to_string(preset) == name) {
                return preset;
            }
        }
        return std::nullopt;
    }

    RaceParams preset_params(RacePreset preset) {
        switch (preset) {
            case RacePreset::Gentle:
                return gentle();
            case RacePreset::Standard:
                // The defaults are Standard, so that a `RaceParams` nobody
                // configured is a difficulty somebody chose.
                return RaceParams{};
            case RacePreset::Brutal:
                return brutal();
        }
        return RaceParams{};
    }

    Result<RaceParams> race_params_from(const RaceConfig& config) {
        if (const std::optional<RacePreset> named = race_preset_from(config.preset); named.has_value()) {
            return preset_params(*named);
        }
        if (config.preset != "custom") {
            // Named rather than silently fallen back to Standard: a typo in a
            // difficulty is somebody playing the wrong game and wondering why.
            return fail(ErrorCode::ConfigInvalid,
                        "race.preset = \"" + config.preset + "\" (expected gentle, standard, brutal or custom)");
        }

        RaceParams params;
        params.ramp_up = config.ramp_up;
        params.ramp_down = config.ramp_down;
        params.min_accuracy = Accuracy{config.min_accuracy};
        params.lead_comfort = static_cast<double>(config.lead_comfort);
        params.lead_danger = static_cast<double>(config.lead_danger);
        params.lead_scale = static_cast<double>(config.lead_scale);
        params.grace = Millis{config.grace_ms};
        params.lives = static_cast<std::size_t>(config.lives);
        params.catch_penalty = config.catch_penalty;
        params.start_factor = config.start_factor;
        params.sustain_window = Millis{config.sustain_window_s * 1'000};

        if (const Status usable = validate(params); !usable) {
            return std::unexpected{usable.error()};
        }
        return params;
    }

    Status validate(const RaceParams& params) {
        // Relationships, not ranges. Each field's own bounds are a config
        // file's business and are checked there; these are the ones that are
        // about two fields at once and would still be wrong in a set nobody
        // wrote in a file.
        if (params.lead_danger >= params.lead_comfort) {
            return fail(ErrorCode::ConfigInvalid,
                        "race.lead_danger = " + number(params.lead_danger) +
                                " (expected less than race.lead_comfort = " + number(params.lead_comfort) + ")");
        }
        if (params.min_accuracy.value <= 0.0 || params.min_accuracy.value >= 1.0) {
            // Open at both ends: a race that demands perfection is unplayable,
            // and one that demands nothing is not teaching anybody to type.
            return fail(ErrorCode::ConfigInvalid,
                        "race.min_accuracy = " + number(params.min_accuracy.value) + " (expected between 0 and 1)");
        }
        if (params.ramp_up <= 0.0) {
            // A race that cannot speed up is a timed run with a ghost in it.
            return fail(ErrorCode::ConfigInvalid,
                        "race.ramp_up = " + number(params.ramp_up) + " (expected greater than 0)");
        }
        if (params.ramp_down < 0.0) {
            return fail(ErrorCode::ConfigInvalid,
                        "race.ramp_down = " + number(params.ramp_down) + " (expected 0 or greater)");
        }
        if (params.lives < 1) {
            // Zero lives is a run that ends before it starts.
            return fail(ErrorCode::ConfigInvalid, "race.lives = 0 (expected 1 or more)");
        }
        if (params.min_speed >= params.max_speed) {
            return fail(ErrorCode::ConfigInvalid,
                        "race.min_speed = " + number(params.min_speed.value) +
                                " (expected less than race.max_speed = " + number(params.max_speed.value) + ")");
        }
        if (params.catch_penalty < 0.0 || params.catch_penalty >= 1.0) {
            // A penalty of 1 sets the speed to zero and the race stops being
            // one; a negative penalty rewards being caught.
            return fail(ErrorCode::ConfigInvalid,
                        "race.catch_penalty = " + number(params.catch_penalty) + " (expected between 0 and 1)");
        }
        if (params.start_factor <= 0.0 || params.start_factor > 1.0) {
            // Above 1 would start the next race faster than anything the
            // typist has ever held, which is the opposite of the progression
            // this is for.
            return fail(ErrorCode::ConfigInvalid,
                        "race.start_factor = " + number(params.start_factor) + " (expected between 0 and 1)");
        }
        return {};
    }

}  // namespace typeit::core
