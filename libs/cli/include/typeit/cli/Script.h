// A typed run, written down (TI-075, TESTING §7).
//
// The input `--simulate` replays. A line is a timestamp and a keystroke, so a
// run that took thirty seconds is a file that takes no time at all to replay
// and produces exactly the same metrics every time.
//
//     # typeit-script 1
//     100  type h
//     250  type e
//     400  backspace
//     520  type e
//     700  type space
//
// The header is required and versioned per VERSIONING §5: a script written for
// a later format is refused rather than half-understood, because a format
// change that silently reinterprets old scripts would quietly invalidate every
// fixture in the test suite.
//
// Timestamps are milliseconds from the start of the run, and must not go
// backwards — the keystroke log's own precondition, checked here where the
// input comes from a file rather than from a keyboard that cannot travel back
// in time.
//
// `type` takes exactly one grapheme, so `type é` is one keystroke and not two,
// and `type hello` is a mistake rather than five. A space is spelled `space`,
// because trailing whitespace in a fixture is invisible and the first editor
// to touch the file would strip it.
#ifndef TYPEIT_CLI_SCRIPT_H
#define TYPEIT_CLI_SCRIPT_H

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

#include "typeit/core/text/Grapheme.h"
#include "typeit/core/util/Result.h"
#include "typeit/core/util/Units.h"

namespace typeit::cli {

    /// The script format this binary understands. Bumped on a format change,
    /// never reused (VERSIONING §5).
    inline constexpr int kScriptVersion = 1;

    struct ScriptEvent {
        /// Milliseconds from the start of the run.
        core::Millis at{0};
        /// What was typed. Empty exactly when this is a backspace, which is
        /// the same convention `core::Keystroke` uses.
        core::Grapheme typed{};

        [[nodiscard]] bool is_backspace() const noexcept { return typed.length == 0; }
    };

    struct Script {
        int version = kScriptVersion;
        std::vector<ScriptEvent> events;
    };

    /// Fails with `ErrorCode::InvalidScript`, naming the line, on anything it
    /// cannot replay exactly: a missing or unsupported header, an unknown
    /// verb, a timestamp that is not a number or that goes backwards, or a
    /// `type` of anything other than one grapheme.
    ///
    /// An empty script — a header and nothing else — is valid. It is a run in
    /// which nobody typed, which is a thing that happens.
    [[nodiscard]] core::Result<Script> parse_script(std::string_view text);

}  // namespace typeit::cli

#endif  // TYPEIT_CLI_SCRIPT_H
