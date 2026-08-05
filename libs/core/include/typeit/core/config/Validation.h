// What counts as a usable setting (TECHNICAL section 6).
//
// Pure: same Config in, same answer out, nothing mutated and nothing read from
// the world. Parsing is infra's job; deciding that a 0-second run is not a run
// is a domain rule, and keeping it here means the same rule applies to a config
// file, a command-line override and a settings screen.
#ifndef TYPEIT_CORE_CONFIG_VALIDATION_H
#define TYPEIT_CORE_CONFIG_VALIDATION_H

#include <array>
#include <cstdint>
#include <string_view>

#include "typeit/core/config/Config.h"
#include "typeit/core/util/Result.h"

namespace typeit::core {

    /// An inclusive range a setting has to fall in.
    struct Range {
        std::int64_t low;
        std::int64_t high;
    };

    // The few limits that are not only a config file's business: `--time` and
    // `--words` override exactly these settings, and a command line that
    // accepted a value the file would reject would be a second, quieter set of
    // rules. Named here, spelled once, used by both.
    inline constexpr Range kDurationSeconds{.low = 1, .high = 3'600};
    inline constexpr Range kWordCount{.low = 1, .high = 10'000};

    /// The modes a run can be started in. The registry decides what is
    /// *registered*; this is what the vocabulary is.
    inline constexpr std::array<std::string_view, 6> kModeNames{"timed", "words", "quote", "zen", "endless", "race"};

    /// The race presets a user can name. `custom` is what a configuration file
    /// becomes once it overrides one, not something to ask for by name — which
    /// is why the configuration accepts one more spelling than this.
    inline constexpr std::array<std::string_view, 3> kRacePresetNames{"gentle", "standard", "brutal"};

    /// The first thing wrong with `config`, named — `"general.default_duration_s
    /// = 0 (expected 1..3600)"` — or success.
    ///
    /// The first rather than all of them: a config file is edited by a person,
    /// who fixes one thing at a time, and a wall of complaints about a file
    /// with one typo in it is worse than a sentence about the typo.
    [[nodiscard]] Status validate(const Config& config);

}  // namespace typeit::core

#endif  // TYPEIT_CORE_CONFIG_VALIDATION_H
