// What counts as a usable setting (TECHNICAL section 6).
//
// Pure: same Config in, same answer out, nothing mutated and nothing read from
// the world. Parsing is infra's job; deciding that a 0-second run is not a run
// is a domain rule, and keeping it here means the same rule applies to a config
// file, a command-line override and a settings screen.
#ifndef TYPEIT_CORE_CONFIG_VALIDATION_H
#define TYPEIT_CORE_CONFIG_VALIDATION_H

#include "typeit/core/config/Config.h"
#include "typeit/core/util/Result.h"

namespace typeit::core {

    /// The first thing wrong with `config`, named — `"general.default_duration_s
    /// = 0 (expected 1..3600)"` — or success.
    ///
    /// The first rather than all of them: a config file is edited by a person,
    /// who fixes one thing at a time, and a wall of complaints about a file
    /// with one typo in it is worse than a sentence about the typo.
    [[nodiscard]] Status validate(const Config& config);

}  // namespace typeit::core

#endif  // TYPEIT_CORE_CONFIG_VALIDATION_H
