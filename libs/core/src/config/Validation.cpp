#include "typeit/core/config/Validation.h"

#include <cstdint>
#include <initializer_list>
#include <span>
#include <string>
#include <string_view>

#include "typeit/core/config/Config.h"
#include "typeit/core/util/Result.h"

namespace typeit::core {
    namespace {

        std::string joined(std::span<const std::string_view> allowed) {
            std::string list;
            for (const std::string_view value: allowed) {
                if (!list.empty()) {
                    list += ", ";
                }
                list += value;
            }
            return list;
        }

        /// `field = value (expected …)`. The field name and the offending value
        /// both appear, because a message that says only "invalid" sends the
        /// reader back to the file to guess which line it meant.
        Status out_of_range(std::string_view field, std::int64_t value, std::int64_t low, std::int64_t high) {
            return fail(ErrorCode::ConfigInvalid, std::string{field} + " = " + std::to_string(value) + " (expected " +
                                                          std::to_string(low) + ".." + std::to_string(high) + ")");
        }

        Status out_of_range(std::string_view field, double value, double low, double high) {
            return fail(ErrorCode::ConfigInvalid, std::string{field} + " = " + std::to_string(value) + " (expected " +
                                                          std::to_string(low) + ".." + std::to_string(high) + ")");
        }

        Status in_range(std::string_view field, std::int64_t value, std::int64_t low, std::int64_t high) {
            if (value < low || value > high) {
                return out_of_range(field, value, low, high);
            }
            return {};
        }

        Status in_range(std::string_view field, double value, double low, double high) {
            if (!(value >= low) || !(value <= high)) {
                // Written as a negation so that a NaN, which compares false
                // against everything, is caught rather than let through.
                return out_of_range(field, value, low, high);
            }
            return {};
        }

        Status one_of(std::string_view field, const std::string& value, std::span<const std::string_view> allowed) {
            for (const std::string_view candidate: allowed) {
                if (value == candidate) {
                    return {};
                }
            }
            return fail(ErrorCode::ConfigInvalid,
                        std::string{field} + " = \"" + value + "\" (expected one of: " + joined(allowed) + ")");
        }

        /// The same, written the way the checks below read. An initializer list
        /// is contiguous, so this is a view over it rather than a copy of it.
        Status one_of(std::string_view field, const std::string& value,
                      std::initializer_list<std::string_view> allowed) {
            return one_of(field, value, std::span{allowed.begin(), allowed.size()});
        }

        /// The first failure of a list of checks, or success. Written as a
        /// helper so each section below reads as the table it is.
        Status first_failure(std::initializer_list<Status> checks) {
            for (const Status& check: checks) {
                if (!check) {
                    return check;
                }
            }
            return {};
        }

        Status validate_general(const GeneralConfig& general) {
            return first_failure({
                    one_of("general.default_mode", general.default_mode, kModeNames),
                    in_range("general.default_duration_s", general.default_duration_s, kDurationSeconds.low,
                             kDurationSeconds.high),
                    in_range("general.default_word_count", general.default_word_count, kWordCount.low, kWordCount.high),
                    in_range("general.countdown_s", general.countdown_s, 0, 5),
                    one_of("general.log_level", general.log_level, {"off", "error", "warn", "info", "debug"}),
            });
        }

        Status validate_appearance(const AppearanceConfig& appearance) {
            return first_failure({
                    one_of("appearance.color_depth", appearance.color_depth,
                           {"auto", "truecolor", "256", "16", "mono"}),
                    one_of("appearance.glyphs", appearance.glyphs, {"auto", "unicode", "ascii"}),
                    one_of("appearance.caret", appearance.caret, {"block", "underline", "outline", "none"}),
                    one_of("appearance.layout", appearance.layout, {"compact", "comfortable"}),
                    // Zero is "fit the terminal", which is why the floor is not
                    // one.
                    in_range("appearance.line_width", appearance.line_width, 0, 500),
                    in_range("appearance.lines_visible", appearance.lines_visible, 1, 50),
            });
        }

        Status validate_text(const TextConfig& text) {
            return first_failure({
                    in_range("text.tab_width", text.tab_width, 1, 16),
                    in_range("text.chunk_graphemes", text.chunk_graphemes, 50, 100'000),
            });
        }

        Status validate_race(const RaceConfig& race) {
            const Status ranges = first_failure({
                    one_of("race.preset", race.preset, {"gentle", "standard", "brutal", "custom"}),
                    one_of("race.start_policy", race.start_policy, {"from_history", "fixed"}),
                    in_range("race.start_wpm", race.start_wpm, 1, 500),
                    in_range("race.ramp_up", race.ramp_up, 0.001, 100.0),
                    in_range("race.ramp_down", race.ramp_down, 0.0, 100.0),
                    // Open interval: a race that demands perfection is
                    // unplayable, and one that demands nothing is not a race.
                    in_range("race.min_accuracy", race.min_accuracy, 0.001, 0.999),
                    in_range("race.lead_comfort", race.lead_comfort, 1, 10'000),
                    in_range("race.lead_danger", race.lead_danger, 1, 10'000),
                    in_range("race.lead_scale", race.lead_scale, 1, 10'000),
                    in_range("race.grace_ms", race.grace_ms, 0, 10'000),
                    in_range("race.lives", race.lives, 1, 100),
                    in_range("race.sustain_window_s", race.sustain_window_s, 1, 3'600),
            });
            if (!ranges) {
                return ranges;
            }
            // Cross-field: danger has to be nearer than comfort, or the pacer
            // is in danger and comfortable at the same time.
            if (race.lead_danger >= race.lead_comfort) {
                return fail(ErrorCode::ConfigInvalid, "race.lead_danger = " + std::to_string(race.lead_danger) +
                                                              " (expected less than race.lead_comfort = " +
                                                              std::to_string(race.lead_comfort) + ")");
            }
            return {};
        }

        Status validate_history(const HistoryConfig& history) {
            return in_range("history.retention_days", history.retention_days, 0, 36'500);
        }

        Status validate_network(const NetworkConfig& network) {
            return first_failure({
                    in_range("network.timeout_s", network.timeout_s, 1, 600),
                    in_range("network.max_size_mb", network.max_size_mb, 1, 1'000),
            });
        }

    }  // namespace

    Status validate(const Config& config) {
        return first_failure({
                in_range("config_version", config.config_version, 1, 1'000),
                validate_general(config.general),
                validate_appearance(config.appearance),
                validate_text(config.text),
                validate_race(config.race),
                validate_history(config.history),
                validate_network(config.network),
        });
    }

}  // namespace typeit::core
