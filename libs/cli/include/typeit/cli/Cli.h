// The command line, parsed (TI-074, TECHNICAL §7).
//
// Hand-rolled, and deliberately: the surface is twenty-odd flags with no
// sub-commands, no repeated options and no shell completion to generate. A
// dependency for that would be more code to build, license and update than the
// thing it replaces.
//
// Pure. It reads no file, opens no terminal and touches no clock — it turns a
// vector of strings into a validated value or an error, which is what makes
// every case in TECHNICAL §7 a table row in a test rather than a subprocess.
//
// The rule for what belongs here: **anything wrong with the command line is
// caught here**, so that by the time the composition root has a `CliOptions` it
// can act on it without checking it again. A range this rejects is the same
// range the configuration file rejects — `core::kDurationSeconds` and
// `core::kWordCount` are shared for exactly that reason, because a command line
// that accepted what the file refuses would be a second, quieter set of rules.
#ifndef TYPEIT_CLI_CLI_H
#define TYPEIT_CLI_CLI_H

#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>

#include "typeit/core/util/Result.h"
#include "typeit/core/util/Units.h"

namespace typeit::cli {

    /// The one thing an invocation does. Everything else on the command line
    /// modifies it, and asking for two of these at once is an error rather
    /// than a guess about which was meant.
    enum class Action : std::uint8_t {
        /// Play. The default, and the only action that reads the text options.
        Run,
        Import,
        ImportDirectory,
        ImportUrl,
        ListTexts,
        RemoveText,
        Stats,
        Export,
        Simulate,
        Doctor,
        Help,
        Version,
    };

    struct CliOptions {
        Action action = Action::Run;

        // --- How to play. Absent means "whatever the configuration says",
        // which is why these are optionals rather than defaulted values: a
        // flag that was not given must not silently override a setting.
        std::optional<std::string> mode;
        std::optional<std::int64_t> seconds;
        std::optional<std::int64_t> words;
        std::optional<std::string> race_preset;

        // --- What to type.
        /// `--text PATH`, or the positional `FILE`. Typed once, not imported.
        std::optional<std::string> text_path;
        std::optional<core::TextId> text_id;
        std::optional<std::int64_t> section;
        /// The bare `-`. The text comes from standard input.
        bool text_from_stdin = false;

        /// What the action operates on: the path for `--import`,
        /// `--import-dir` and `--simulate`, the URL for `--url`, the format
        /// for `--export`. One field rather than five, because exactly one
        /// action runs and the others have nothing to say.
        std::string operand;
        /// `--remove-text ID`. Its own field rather than `text_id`: removing a
        /// text and typing one are different intentions, and sharing a field
        /// would let `--text-id 3 --remove-text 4` look coherent.
        std::optional<core::TextId> remove_id;
        std::optional<std::int64_t> last;

        // --- Where to look. Both bypass the platform paths entirely, which is
        // what makes an end-to-end test possible in a temporary directory.
        std::optional<std::string> config_path;
        std::optional<std::string> data_dir;
    };

    /// `arguments` is argv **without** the program name.
    ///
    /// `--help` and `--version` short-circuit: they are answered the moment
    /// they are seen, and nothing after them is parsed or complained about. A
    /// user who types a flag wrongly and then asks for help should get help.
    ///
    /// Everything else is validated: unknown flags (with the nearest real one
    /// suggested), missing values, numbers outside their range, values outside
    /// their vocabulary, and combinations that contradict each other. The error
    /// names the flag and, where there is one, the range or the list.
    [[nodiscard]] core::Result<CliOptions> parse(std::span<const std::string_view> arguments);

}  // namespace typeit::cli

#endif  // TYPEIT_CLI_CLI_H
