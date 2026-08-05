#include "typeit/cli/Cli.h"

#include <algorithm>
#include <array>
#include <charconv>
#include <cstddef>
#include <cstdint>
#include <iterator>
#include <limits>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>
#include <vector>

#include "typeit/core/Version.h"
#include "typeit/core/config/Validation.h"
#include "typeit/core/util/Result.h"
#include "typeit/core/util/Units.h"

namespace typeit::cli {
    namespace {

        using core::ErrorCode;
        using core::Range;
        using core::Result;
        using core::Status;

        enum class Flag : std::uint8_t {
            Mode,
            Time,
            Words,
            RacePreset,
            Text,
            TextId,
            Section,
            Import,
            ImportDirectory,
            Url,
            ListTexts,
            RemoveText,
            Stats,
            Export,
            Last,
            Simulate,
            Doctor,
            Config,
            DataDir,
            Help,
            Version,
        };

        struct Option {
            Flag flag;
            /// The long form, without the dashes.
            std::string_view name;
            /// The short form, or `\0` where there is none.
            char letter;
            bool takes_value;
            /// The heading this flag appears under in `--help`, in the order
            /// TECHNICAL section 7 documents.
            std::string_view group;
            /// What the value is called there. Empty for a flag that takes
            /// none.
            std::string_view placeholder;
            std::string_view description;
        };

        /// TECHNICAL §7, in the order it documents. One table: the parser, the
        /// suggester and — when there is a `--help` to print — the help all
        /// read this rather than each carrying their own list to fall out of
        /// step with.
        constexpr std::array kOptions{
                Option{.flag = Flag::Mode,
                       .name = "mode",
                       .letter = 'm',
                       .takes_value = true,
                       .group = "Modes",
                       .placeholder = "<MODE>",
                       .description = "timed | words | quote | zen | endless | race"},
                Option{.flag = Flag::Time,
                       .name = "time",
                       .letter = 't',
                       .takes_value = true,
                       .group = "Modes",
                       .placeholder = "<SECONDS>",
                       .description = "duration for timed mode"},
                Option{.flag = Flag::Words,
                       .name = "words",
                       .letter = 'w',
                       .takes_value = true,
                       .group = "Modes",
                       .placeholder = "<N>",
                       .description = "word count for words mode"},
                Option{.flag = Flag::RacePreset,
                       .name = "race-preset",
                       .letter = '\0',
                       .takes_value = true,
                       .group = "Modes",
                       .placeholder = "<PRESET>",
                       .description = "gentle | standard | brutal"},
                Option{.flag = Flag::Text,
                       .name = "text",
                       .letter = '\0',
                       .takes_value = true,
                       .group = "Text",
                       .placeholder = "<PATH>",
                       .description = "type this file (one-shot; does not import)"},
                Option{.flag = Flag::TextId,
                       .name = "text-id",
                       .letter = '\0',
                       .takes_value = true,
                       .group = "Text",
                       .placeholder = "<ID>",
                       .description = "type a text from the library"},
                Option{.flag = Flag::Section,
                       .name = "section",
                       .letter = '\0',
                       .takes_value = true,
                       .group = "Text",
                       .placeholder = "<N>",
                       .description = "start at section N of the selected text"},
                Option{.flag = Flag::Import,
                       .name = "import",
                       .letter = '\0',
                       .takes_value = true,
                       .group = "Library",
                       .placeholder = "<PATH>",
                       .description = "import into the library and exit"},
                Option{.flag = Flag::ImportDirectory,
                       .name = "import-dir",
                       .letter = '\0',
                       .takes_value = true,
                       .group = "Library",
                       .placeholder = "<DIR>",
                       .description = "import every supported file in a directory"},
                Option{.flag = Flag::Url,
                       .name = "url",
                       .letter = '\0',
                       .takes_value = true,
                       .group = "Library",
                       .placeholder = "<URL>",
                       .description = "fetch and import a web page (requires [network].enabled)"},
                Option{.flag = Flag::ListTexts,
                       .name = "list-texts",
                       .letter = '\0',
                       .takes_value = false,
                       .group = "Library",
                       .placeholder = "",
                       .description = "list library texts and exit"},
                Option{.flag = Flag::RemoveText,
                       .name = "remove-text",
                       .letter = '\0',
                       .takes_value = true,
                       .group = "Library",
                       .placeholder = "<ID>",
                       .description = "remove a text from the library"},
                Option{.flag = Flag::Stats,
                       .name = "stats",
                       .letter = '\0',
                       .takes_value = false,
                       .group = "History",
                       .placeholder = "",
                       .description = "print a summary and exit"},
                Option{.flag = Flag::Export,
                       .name = "export",
                       .letter = '\0',
                       .takes_value = true,
                       .group = "History",
                       .placeholder = "<FORMAT>",
                       .description = "csv | json -- write history to stdout"},
                Option{.flag = Flag::Last,
                       .name = "last",
                       .letter = '\0',
                       .takes_value = true,
                       .group = "History",
                       .placeholder = "<N>",
                       .description = "limit history output"},
                Option{.flag = Flag::Simulate,
                       .name = "simulate",
                       .letter = '\0',
                       .takes_value = true,
                       .group = "Diagnostics",
                       .placeholder = "<SCRIPT>",
                       .description = "run headless from a keystroke script, print metrics"},
                Option{.flag = Flag::Doctor,
                       .name = "doctor",
                       .letter = '\0',
                       .takes_value = false,
                       .group = "Diagnostics",
                       .placeholder = "",
                       .description = "report terminal capabilities, paths, database health"},
                Option{.flag = Flag::Config,
                       .name = "config",
                       .letter = '\0',
                       .takes_value = true,
                       .group = "Diagnostics",
                       .placeholder = "<PATH>",
                       .description = "use an alternative config file"},
                Option{.flag = Flag::DataDir,
                       .name = "data-dir",
                       .letter = '\0',
                       .takes_value = true,
                       .group = "Diagnostics",
                       .placeholder = "<PATH>",
                       .description = "use an alternative data directory"},
                Option{.flag = Flag::Help,
                       .name = "help",
                       .letter = 'h',
                       .takes_value = false,
                       .group = "Diagnostics",
                       .placeholder = "",
                       .description = "print this help and exit"},
                Option{.flag = Flag::Version,
                       .name = "version",
                       .letter = 'V',
                       .takes_value = false,
                       .group = "Diagnostics",
                       .placeholder = "",
                       .description = "print the version and exit"},
        };

        /// A database id is a positive integer, and there is no upper bound
        /// worth inventing — the row either exists or it does not.
        constexpr Range kIdentifier{.low = 1, .high = std::numeric_limits<std::int64_t>::max()};
        /// Sections and history pages are counted from one, because the user
        /// typed the number and nobody asks for the zeroth section.
        constexpr Range kPositiveCount{.low = 1, .high = std::numeric_limits<std::int64_t>::max()};

        std::string flagged(std::string_view name) { return "--" + std::string{name}; }

        std::unexpected<core::Error> refuse(std::string context) {
            return std::unexpected{core::make_error(ErrorCode::InvalidArgument, std::move(context))};
        }

        /// Levenshtein, on the cold path only — nothing calls this until
        /// somebody has already mistyped a flag.
        std::size_t edit_distance(std::string_view left, std::string_view right) {
            std::vector<std::size_t> previous(right.size() + 1);
            std::vector<std::size_t> current(right.size() + 1);
            // Filled by hand rather than with iota: `std::ranges::iota` is
            // C++23 and libc++ does not have it, and the numbered version trips
            // modernize-use-ranges. A loop is portable and shorter than the
            // argument about it.
            for (std::size_t j = 0; j <= right.size(); ++j) {
                previous.at(j) = j;
            }

            for (std::size_t i = 0; i < left.size(); ++i) {
                current.at(0) = i + 1;
                for (std::size_t j = 0; j < right.size(); ++j) {
                    const std::size_t substitution = previous.at(j) + (left.at(i) == right.at(j) ? 0U : 1U);
                    current.at(j + 1) = std::min({current.at(j) + 1, previous.at(j + 1) + 1, substitution});
                }
                previous.swap(current);
            }
            return previous.at(right.size());
        }

        /// The flag `name` was probably meant to be, or nothing.
        ///
        /// Two edits at most. Further than that and the suggestion is noise:
        /// `--xyzzy` has no near miss, and offering the alphabetically luckiest
        /// flag would send the reader off to read about something they never
        /// asked for.
        std::optional<std::string_view> nearest(std::string_view name) {
            std::size_t best = 3;
            std::optional<std::string_view> match;
            for (const Option& option: kOptions) {
                if (const std::size_t distance = edit_distance(name, option.name); distance < best) {
                    best = distance;
                    match = option.name;
                }
            }
            return match;
        }

        // `const auto`, not the `const auto* const` clang-tidy asks for:
        // libstdc++'s std::array iterator is a pointer and MSVC's is a class,
        // so the qualified spelling does not compile on Windows.
        const Option* find_long(std::string_view name) {
            // NOLINTNEXTLINE(readability-qualified-auto) -- MSVC's array iterator is not a pointer
            const auto found = std::ranges::find(kOptions, name, &Option::name);
            return found == kOptions.end() ? nullptr : &*found;
        }

        const Option* find_short(char letter) {
            // NOLINTNEXTLINE(readability-qualified-auto) -- MSVC's array iterator is not a pointer
            const auto found = std::ranges::find(kOptions, letter, &Option::letter);
            return found == kOptions.end() ? nullptr : &*found;
        }

        Result<std::int64_t> as_number(std::string_view name, std::string_view text, Range range) {
            std::int64_t value = 0;
            // `data() + size()` is the [first, last) from_chars wants, so the
            // size is carried rather than assumed — which is what the
            // string-view check is asking for and cannot see.
            // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-pointer-arithmetic) -- the pair is the range
            const char* const last = text.data() + text.size();
            // NOLINTNEXTLINE(bugprone-suspicious-stringview-data-usage) -- `last` is the size
            const std::from_chars_result parsed = std::from_chars(text.data(), last, value);
            // `ptr != last` catches "30s" and "3.5", which from_chars is happy
            // to read the front of and leave the rest.
            if (parsed.ec != std::errc{} || parsed.ptr != last) {
                return refuse(flagged(name) + " = \"" + std::string{text} + "\" (expected a whole number)");
            }
            if (value < range.low || value > range.high) {
                return refuse(flagged(name) + " = " + std::to_string(value) + " (expected " +
                              std::to_string(range.low) + ".." + std::to_string(range.high) + ")");
            }
            return value;
        }

        Status one_of(std::string_view name, std::string_view value, std::span<const std::string_view> vocabulary) {
            if (std::ranges::find(vocabulary, value) != vocabulary.end()) {
                return {};
            }
            std::string list;
            for (const std::string_view allowed: vocabulary) {
                if (!list.empty()) {
                    list += ", ";
                }
                list += allowed;
            }
            return refuse(flagged(name) + " = \"" + std::string{value} + "\" (expected one of: " + list + ")");
        }

        /// What the parser carries between arguments and has to check at the
        /// end. Kept beside the options rather than inside them: which flag
        /// claimed the action is a fact about the command line, not about the
        /// program's configuration.
        struct State {
            CliOptions options;
            /// The flag that chose the action, so a second one can name both in
            /// its complaint.
            std::optional<std::string_view> action_flag;
            /// How the text was named, for the same reason. A string rather
            /// than a flag: the second half of "a.txt and b.txt cannot be
            /// combined" is a filename, and dressing it up as `--a.txt` would
            /// be worse than saying nothing.
            std::optional<std::string> text_source;
            /// `--help` or `--version` was seen; nothing after it matters.
            bool answered = false;
        };

        Status claim_action(State& state, Action action, std::string_view name) {
            if (state.action_flag.has_value()) {
                return refuse(flagged(*state.action_flag) + " and " + flagged(name) + " cannot be combined");
            }
            state.action_flag = name;
            state.options.action = action;
            return {};
        }

        Status claim_text(State& state, std::string named) {
            if (state.text_source.has_value()) {
                return refuse(*state.text_source + " and " + named + " cannot be combined");
            }
            state.text_source = std::move(named);
            return {};
        }

        /// An action and the one thing it operates on.
        Status take_operand(State& state, const Option& option, Action action, std::string_view value) {
            if (const Status only = claim_action(state, action, option.name); !only) {
                return only;
            }
            state.options.operand = std::string{value};
            return {};
        }

        Status apply(State& state, const Option& option, std::string_view value) {
            CliOptions& options = state.options;
            switch (option.flag) {
                case Flag::Mode:
                    if (const Status known = one_of(option.name, value, core::kModeNames); !known) {
                        return known;
                    }
                    options.mode = std::string{value};
                    return {};

                case Flag::Time: {
                    const Result<std::int64_t> seconds = as_number(option.name, value, core::kDurationSeconds);
                    if (!seconds) {
                        return std::unexpected{seconds.error()};
                    }
                    options.seconds = *seconds;
                    return {};
                }

                case Flag::Words: {
                    const Result<std::int64_t> words = as_number(option.name, value, core::kWordCount);
                    if (!words) {
                        return std::unexpected{words.error()};
                    }
                    options.words = *words;
                    return {};
                }

                case Flag::RacePreset:
                    if (const Status known = one_of(option.name, value, core::kRacePresetNames); !known) {
                        return known;
                    }
                    options.race_preset = std::string{value};
                    return {};

                case Flag::Text:
                    if (const Status only = claim_text(state, flagged(option.name)); !only) {
                        return only;
                    }
                    options.text_path = std::string{value};
                    return {};

                case Flag::TextId: {
                    if (const Status only = claim_text(state, flagged(option.name)); !only) {
                        return only;
                    }
                    const Result<std::int64_t> id = as_number(option.name, value, kIdentifier);
                    if (!id) {
                        return std::unexpected{id.error()};
                    }
                    options.text_id = core::TextId{*id};
                    return {};
                }

                case Flag::Section: {
                    const Result<std::int64_t> section = as_number(option.name, value, kPositiveCount);
                    if (!section) {
                        return std::unexpected{section.error()};
                    }
                    options.section = *section;
                    return {};
                }

                // Four actions that differ only in which one they are: each
                // takes a path or a URL, keeps it verbatim, and leaves opening
                // it to the layer that knows how.
                case Flag::Import:
                    return take_operand(state, option, Action::Import, value);
                case Flag::ImportDirectory:
                    return take_operand(state, option, Action::ImportDirectory, value);
                case Flag::Url:
                    return take_operand(state, option, Action::ImportUrl, value);
                case Flag::Simulate:
                    return take_operand(state, option, Action::Simulate, value);

                case Flag::ListTexts:
                    return claim_action(state, Action::ListTexts, option.name);

                case Flag::RemoveText: {
                    if (const Status only = claim_action(state, Action::RemoveText, option.name); !only) {
                        return only;
                    }
                    const Result<std::int64_t> id = as_number(option.name, value, kIdentifier);
                    if (!id) {
                        return std::unexpected{id.error()};
                    }
                    options.remove_id = core::TextId{*id};
                    return {};
                }

                case Flag::Stats:
                    return claim_action(state, Action::Stats, option.name);

                case Flag::Export: {
                    static constexpr std::array<std::string_view, 2> kFormats{"csv", "json"};
                    if (const Status known = one_of(option.name, value, kFormats); !known) {
                        return known;
                    }
                    return take_operand(state, option, Action::Export, value);
                }

                case Flag::Last: {
                    const Result<std::int64_t> last = as_number(option.name, value, kPositiveCount);
                    if (!last) {
                        return std::unexpected{last.error()};
                    }
                    options.last = *last;
                    return {};
                }

                case Flag::Doctor:
                    return claim_action(state, Action::Doctor, option.name);

                case Flag::Config:
                    options.config_path = std::string{value};
                    return {};

                case Flag::DataDir:
                    options.data_dir = std::string{value};
                    return {};

                case Flag::Help:
                    options.action = Action::Help;
                    state.answered = true;
                    return {};

                case Flag::Version:
                    options.action = Action::Version;
                    state.answered = true;
                    return {};
            }
            // Unreachable: every enumerator returns above. One line is cheaper
            // than the undefined behaviour of falling off the end if the enum
            // ever holds a value cast in from outside.
            return {};
        }

        /// The bare `FILE` or `-`, which mean the same as `--text PATH` and
        /// standard input.
        ///
        /// After `--` a lone `-` is the file called `-`, not standard input.
        /// Otherwise `--` would give no way at all to name that file, which is
        /// the only thing `--` is for.
        Status take_positional(State& state, std::string_view argument, bool options_ended) {
            if (const Status only = claim_text(state, std::string{argument}); !only) {
                return only;
            }
            if (argument == "-" && !options_ended) {
                state.options.text_from_stdin = true;
            } else {
                state.options.text_path = std::string{argument};
            }
            return {};
        }

        /// One argument, taken apart: which option it names, and the value
        /// attached with an `=` if there was one.
        struct Resolved {
            /// Null when nothing by that name is registered.
            const Option* option = nullptr;
            /// The name as written, without the dashes — what the suggester
            /// measures against and what the complaint quotes.
            std::string_view name;
            std::optional<std::string_view> inline_value;
        };

        Resolved resolve(std::string_view argument) {
            Resolved resolved;
            if (argument.starts_with("--")) {
                resolved.name = argument.substr(2);
                if (const std::size_t equals = resolved.name.find('='); equals != std::string_view::npos) {
                    resolved.inline_value = resolved.name.substr(equals + 1);
                    resolved.name = resolved.name.substr(0, equals);
                }
                resolved.option = find_long(resolved.name);
                return resolved;
            }
            // A short flag is one letter. Clustering (`-hV`) is not supported
            // and is reported as the unknown flag it is, rather than
            // half-understood.
            resolved.name = argument.substr(1);
            resolved.option = resolved.name.size() == 1 ? find_short(resolved.name.front()) : nullptr;
            return resolved;
        }

        using Cursor = std::span<const std::string_view>::iterator;

        /// The value the option was given, consuming the next argument when it
        /// was given separately. Advances `at` when it does.
        Result<std::string_view> value_for(const Resolved& resolved, Cursor& at, Cursor end) {
            const Option& option = *resolved.option;
            if (!option.takes_value) {
                if (resolved.inline_value.has_value()) {
                    return refuse(flagged(option.name) + " takes no value");
                }
                return std::string_view{};
            }
            if (resolved.inline_value.has_value()) {
                return *resolved.inline_value;
            }
            if (std::next(at) == end) {
                return refuse(flagged(option.name) + " expects a value");
            }
            return *++at;
        }

    }  // namespace

    core::Result<CliOptions> parse(std::span<const std::string_view> arguments) {
        State state;
        bool options_ended = false;

        for (auto argument_at = arguments.begin(); argument_at != arguments.end(); ++argument_at) {
            const std::string_view argument = *argument_at;

            if (!options_ended && argument == "--") {
                // Everything after this is a filename, however many dashes it
                // starts with.
                options_ended = true;
                continue;
            }

            const bool is_option = !options_ended && argument.size() > 1 && argument.starts_with('-');
            if (!is_option) {
                if (const Status taken = take_positional(state, argument, options_ended); !taken) {
                    return std::unexpected{taken.error()};
                }
                continue;
            }

            const Resolved resolved = resolve(argument);
            if (resolved.option == nullptr) {
                const std::optional<std::string_view> suggestion = nearest(resolved.name);
                const std::string guess = suggestion.has_value() ? " (did you mean " + flagged(*suggestion) + "?)" : "";
                return refuse("unknown option " + std::string{argument} + guess);
            }

            const Result<std::string_view> value = value_for(resolved, argument_at, arguments.end());
            if (!value) {
                return std::unexpected{value.error()};
            }

            if (const Status applied = apply(state, *resolved.option, *value); !applied) {
                return std::unexpected{applied.error()};
            }
            if (state.answered) {
                // `--help` and `--version` answer immediately, and nothing
                // after them is parsed: somebody who has just mistyped a flag
                // and asked for help should get help.
                CliOptions answered;
                answered.action = state.options.action;
                return answered;
            }
        }

        return state.options;
    }

    std::string usage() {
        // The width the descriptions line up at. Wide enough for the longest
        // flag the table holds, checked by a test rather than by eye — a help
        // text whose columns drift as flags are added is a help text nobody
        // trusts.
        constexpr std::size_t kDescriptionColumn = 29;

        std::string out = "typeit [OPTIONS] [FILE|-]\n";
        std::string_view group;

        for (const Option& option: kOptions) {
            if (option.group != group) {
                group = option.group;
                out += '\n';
                out += group;
                out += '\n';
            }

            std::string spelled = "  ";
            if (option.letter != '\0') {
                spelled += '-';
                spelled += option.letter;
                spelled += ", ";
            } else {
                spelled += "    ";
            }
            spelled += flagged(option.name);
            if (!option.placeholder.empty()) {
                spelled += ' ';
                spelled += option.placeholder;
            }

            // One space at minimum, however long the flag ran: a description
            // pushed onto the next line is worse than a ragged column.
            spelled.append(spelled.size() >= kDescriptionColumn ? 1 : kDescriptionColumn - spelled.size(), ' ');
            out += spelled;
            out += option.description;
            out += '\n';
        }

        // The positional, which is not an option and so is not in the table.
        out += "\n  -                            read the text from stdin\n";
        return out;
    }

    std::string version_text() {
        std::string out = "typeit ";
        out += kVersionString;
        if (const std::string_view describe = kGitDescribe; !describe.empty()) {
            out += " (";
            out += describe;
            out += ")";
        }
        // NDEBUG rather than a configure-time string: under a multi-config
        // generator the build type is not known until the compiler runs, so a
        // value baked in at configure time would be a guess.
#ifdef NDEBUG
        out += " release";
#else
        out += " debug";
#endif
        out += '\n';
        return out;
    }

}  // namespace typeit::cli
