// The three difficulties, and turning a config file into a set of numbers
// (TI-123, GAMEPLAY section 3.5).
//
// The table is the document's, transcribed once. A test reads it back against
// the same values so that a difficulty cannot be edited here without somebody
// editing GAMEPLAY too — and, more usefully, so that the *ordering* between the
// three cannot silently invert. "Gentle" that ramps faster than "Brutal" is a
// bug nobody would report, because both still work.
#ifndef TYPEIT_CORE_RACE_RACEPRESETS_H
#define TYPEIT_CORE_RACE_RACEPRESETS_H

#include <cstdint>
#include <optional>
#include <span>
#include <string_view>

#include "typeit/core/config/Config.h"
#include "typeit/core/race/RaceParams.h"
#include "typeit/core/util/Result.h"

namespace typeit::core {

    enum class RacePreset : std::uint8_t {
        Gentle,
        Standard,
        Brutal,
    };

    [[nodiscard]] std::string_view to_string(RacePreset preset);

    /// The named presets, in ascending difficulty. `custom` is deliberately
    /// absent: it is not a difficulty, it is the absence of one.
    [[nodiscard]] std::span<const RacePreset> race_presets();

    /// `nullopt` for anything else, including `"custom"` — which is a real
    /// configuration value and not a preset, so the two questions stay apart.
    [[nodiscard]] std::optional<RacePreset> race_preset_from(std::string_view name);

    [[nodiscard]] RaceParams preset_params(RacePreset preset);

    /// The numbers a race will actually be run by.
    ///
    /// A named preset uses the table and ignores the explicit values, which is
    /// what makes "standard" mean the same thing in every config file. Only
    /// `preset = "custom"` reads them — otherwise a stale `ramp_up` left in a
    /// file from an afternoon of experimenting would quietly follow somebody
    /// into every preset they chose afterwards.
    [[nodiscard]] Result<RaceParams> race_params_from(const RaceConfig& config);

    /// The relationships that have to hold between the numbers, whatever their
    /// individual ranges.
    ///
    /// Separate from `validate(const Config&)`, which checks a config file: a
    /// preset, a command line and a settings screen can all produce a
    /// `RaceParams`, and only one of those three is a file.
    [[nodiscard]] Status validate(const RaceParams& params);

}  // namespace typeit::core

#endif  // TYPEIT_CORE_RACE_RACEPRESETS_H
