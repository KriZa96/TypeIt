// A run, played from a file (TI-075, TESTING §7).
//
// The whole point of Phase 3: a complete typing session — resolved, played,
// measured and persisted — with no terminal anywhere in the call stack. It is
// the same `app::SessionService` and the same `core::Session` a real run goes
// through, driven by timestamps from a script instead of timestamps from a
// keyboard. Nothing here is a test double, which is what makes it an
// end-to-end test rather than a second implementation of the game.
//
// It is also the tool for looking at what a run *did* without playing it:
// Phase 7's race ramp is a curve nobody can read while typing.
#ifndef TYPEIT_CLI_SIMULATE_H
#define TYPEIT_CLI_SIMULATE_H

#include <string>

#include "typeit/app/services/SessionService.h"
#include "typeit/cli/Script.h"
#include "typeit/core/util/Result.h"

namespace typeit::cli {

    /// The schema version of the JSON below. Bumped when a field changes
    /// meaning or leaves, so a consumer can tell.
    inline constexpr int kSimulationSchema = 1;

    /// Plays `script` through `service` and returns the run's metrics as JSON.
    ///
    /// The run is driven exactly as a screen would drive it: time is offered to
    /// the mode before each keystroke, and once the mode says the run is over
    /// no further keystroke is delivered — a script longer than the run does
    /// not get to keep typing past the end, any more than a typist would.
    ///
    /// A script that runs out before the mode finishes is an abandoned run, and
    /// is saved as one. That is what happens when somebody stops typing.
    ///
    /// The output carries **no wall-clock timestamps**. Everything in it is
    /// derived from the script's own relative times, which is what lets the
    /// same script produce byte-identical output on two machines a year apart.
    /// The run's date is still recorded in the database, where it belongs.
    [[nodiscard]] core::Result<std::string> simulate(const app::SessionService& service,
                                                     const app::SessionRequest& request, const Script& script);

}  // namespace typeit::cli

#endif  // TYPEIT_CLI_SIMULATE_H
