// The composition root (ARCHITECTURE §4.5).
//
// The one translation unit that names every layer, and the only place any of
// them is constructed. Everything below it takes what it needs as a parameter,
// which is what makes the rest of this program testable without a database, a
// terminal or a clock.
//
// It does four things: parse the arguments, construct the adapters, hand them
// to a use case, and print what came back. There is no logic here to test,
// deliberately — anything worth a test belongs one layer down, where a test
// can reach it without spawning a process.
//
// The actions that are not built yet say so plainly rather than doing nothing.

#include <csignal>
#include <cstddef>
#include <cstdlib>
#include <exception>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

// `isatty` is the one thing here with no portable spelling: POSIX puts it in
// <unistd.h>, the MSVC runtime puts `_isatty` in <io.h>. Included explicitly
// rather than relied on transitively, because "it compiled on my libstdc++" is
// how a header goes missing on the other compiler.
#ifdef _WIN32
#include <io.h>
#else
#include <unistd.h>
#endif

#include "typeit/app/Theme.h"
#include "typeit/app/ports/IConfigStore.h"
#include "typeit/app/services/HistoryService.h"
#include "typeit/app/services/SessionService.h"
#include "typeit/cli/Cli.h"
#include "typeit/cli/Reports.h"
#include "typeit/cli/Script.h"
#include "typeit/cli/Simulate.h"
#include "typeit/core/config/Config.h"
#include "typeit/core/modes/ModeRegistry.h"
#include "typeit/core/modes/QuoteMode.h"
#include "typeit/core/modes/TimedMode.h"
#include "typeit/core/modes/WordCountMode.h"
#include "typeit/core/modes/ZenMode.h"
#include "typeit/core/util/Result.h"
#include "typeit/core/util/Units.h"
#include "typeit/infra/Doctor.h"
#include "typeit/infra/config/TomlConfigStore.h"
#include "typeit/infra/db/Migrator.h"
#include "typeit/infra/db/SqliteDatabase.h"
#include "typeit/infra/db/SqliteHistoryRepository.h"
#include "typeit/infra/fs/AssetLocator.h"
#include "typeit/infra/fs/PlatformPaths.h"
#include "typeit/infra/term/Capabilities.h"
#include "typeit/infra/theme/ThemeLoader.h"
#include "typeit/infra/time/SystemClock.h"
#include "typeit/tui/TerminalApp.h"

namespace {

    using typeit::core::Result;
    using typeit::core::Status;

    /// The exit codes this program uses. Two is the shell convention for "you
    /// asked for something impossible", one for "I tried and could not".
    constexpr int kOk = 0;
    constexpr int kFailed = 1;
    constexpr int kMisused = 2;

    int complain(const typeit::core::Error& error) {
        std::cerr << "typeit: " << typeit::core::to_string(error) << '\n';
        return error.code == typeit::core::ErrorCode::InvalidArgument ? kMisused : kFailed;
    }

    int print(std::string_view text) {
        std::cout << text;
        // Checked, because a closed stdout is not a reason to claim success:
        // `typeit --export json | head -1` should not report that it exported
        // everything.
        return std::cout.good() ? kOk : kFailed;
    }

    /// The modes a run can be started in, each closed over the configured
    /// default it falls back to. The caller's choice wins; the configuration is
    /// what a caller with nothing to say gets.
    typeit::core::ModeRegistry built_in_modes(const typeit::core::Config& config) {
        typeit::core::ModeRegistry registry;
        // The configured value is the default, and whatever the caller chose
        // wins. Baking the configured one in was the bug: the menu's seconds
        // field changed the number written to the history and nothing else.
        registry.register_mode("timed", [fallback = typeit::core::Millis{config.general.default_duration_s * 1'000}](
                                                const typeit::core::ModeParams& params) {
            return std::make_unique<typeit::core::TimedMode>(params.duration.value > 0 ? params.duration : fallback);
        });
        registry.register_mode("words", [fallback = static_cast<std::size_t>(config.general.default_word_count)](
                                                const typeit::core::ModeParams& params) {
            return std::make_unique<typeit::core::WordCountMode>(params.words > 0 ? params.words : fallback);
        });
        registry.register_mode("quote", [] { return std::make_unique<typeit::core::QuoteMode>(); });
        registry.register_mode("zen", [] { return std::make_unique<typeit::core::ZenMode>(); });
        return registry;
    }

    /// Everything that needs a database, opened and migrated once.
    ///
    /// A database from a newer binary is refused here rather than upgraded:
    /// somebody's entire typing history is in there.
    struct History {
        typeit::infra::SqliteDatabase database;
        typeit::infra::SqliteHistoryRepository repository;

        explicit History(typeit::infra::SqliteDatabase&& opened) : database{std::move(opened)}, repository{database} {}
    };

    Result<std::unique_ptr<History>> open_history(const std::filesystem::path& data_directory) {
        if (const Status made = typeit::infra::ensure_directory(data_directory); !made) {
            return std::unexpected{made.error()};
        }
        Result<typeit::infra::SqliteDatabase> database = typeit::infra::SqliteDatabase::open(
                data_directory / "typeit.db", typeit::infra::SqliteDatabase::OpenMode::CreateIfMissing);
        if (!database) {
            return std::unexpected{database.error()};
        }
        auto history = std::make_unique<History>(std::move(*database));
        if (const Result<typeit::infra::MigrationOutcome> migrated =
                    typeit::infra::migrate_to_latest(history->database);
            !migrated) {
            return std::unexpected{migrated.error()};
        }
        return history;
    }

    Result<std::string> read_file(const std::filesystem::path& path) {
        const std::ifstream file{path, std::ios::binary};
        if (!file) {
            return typeit::core::fail(typeit::core::ErrorCode::FileNotFound, path.string());
        }
        std::ostringstream contents;
        contents << file.rdbuf();
        return contents.str();
    }

    /// Where the data lives: `--data-dir` if it was given, the platform's
    /// answer otherwise.
    Result<std::filesystem::path> data_directory(const typeit::cli::CliOptions& options,
                                                 const typeit::infra::Environment& environment) {
        if (options.data_dir.has_value()) {
            return std::filesystem::path{*options.data_dir};
        }
        const Result<typeit::infra::Paths> paths = typeit::infra::resolve_paths(environment);
        if (!paths) {
            return std::unexpected{paths.error()};
        }
        return paths->data;
    }

    int run_doctor(const typeit::infra::Environment& environment) {
        const Result<std::filesystem::path> executable = typeit::infra::current_executable();
        std::error_code failed;
        const typeit::infra::Examination examination{
                .environment = environment,
                .executable = executable.value_or(std::filesystem::path{}),
                .working_directory = std::filesystem::current_path(failed),
                .install_prefix = typeit::infra::configured_install_prefix(),
        };
        return print(typeit::infra::to_text(typeit::infra::diagnose(examination)));
    }

    int run_history_action(const typeit::cli::CliOptions& options, const typeit::infra::Environment& environment) {
        const Result<std::filesystem::path> directory = data_directory(options, environment);
        if (!directory) {
            return complain(directory.error());
        }
        const Result<std::unique_ptr<History>> history = open_history(*directory);
        if (!history) {
            return complain(history.error());
        }

        const typeit::app::HistoryService service{(*history)->repository};
        const Result<std::string> text = options.action == typeit::cli::Action::Stats
                                                 ? typeit::cli::stats_summary(service, (*history)->repository, options,
                                                                              typeit::infra::unix_now())
                                                 : typeit::cli::export_history(service, options);
        if (!text) {
            return complain(text.error());
        }
        return print(*text);
    }

    int run_simulate(const typeit::cli::CliOptions& options, const typeit::infra::Environment& environment) {
        const Result<std::string> source = read_file(options.operand);
        if (!source) {
            return complain(source.error());
        }
        const Result<typeit::cli::Script> script = typeit::cli::parse_script(*source);
        if (!script) {
            return complain(script.error());
        }

        const Result<std::filesystem::path> directory = data_directory(options, environment);
        if (!directory) {
            return complain(directory.error());
        }
        const Result<std::unique_ptr<History>> history = open_history(*directory);
        if (!history) {
            return complain(history.error());
        }

        // The real services, the real modes and a real database — the whole
        // point of the exercise. The only thing a scripted run does not use is
        // a terminal.
        const typeit::core::Config config;
        const typeit::core::ModeRegistry modes = built_in_modes(config);
        const typeit::infra::SystemClock clock;
        const typeit::app::SessionService sessions{(*history)->repository, modes, clock, clock};

        typeit::app::SessionRequest request;
        request.mode = options.mode.value_or(config.general.default_mode);
        request.mode_param = R"({"source":"simulate"})";
        request.rules = config.typing;

        if (options.text_path.has_value()) {
            const Result<std::string> text = read_file(*options.text_path);
            if (!text) {
                return complain(text.error());
            }
            request.text = *text;
        } else {
            // Something to type, so that `--simulate` needs only a script. The
            // library is Phase 6's; until then a script that wants its own text
            // passes `--text`.
            request.text = "the quick brown fox jumps over the lazy dog";
        }

        const Result<std::string> output = typeit::cli::simulate(sessions, request, *script);
        if (!output) {
            return complain(output.error());
        }
        return print(*output + "\n");
    }

    /// Whether there is a terminal to draw on.
    ///
    /// Without one FTXUI's loop waits for input that will never arrive, so
    /// `typeit < /dev/null` hangs rather than ending — which is worse than
    /// saying plainly that this mode needs a terminal. Everything that does
    /// not need one (`--simulate`, `--stats`, `--export`, `--doctor`) works
    /// redirected, which is the whole point of Phase 3.
    bool has_a_terminal() {
#ifdef _WIN32
        return _isatty(_fileno(stdin)) != 0 && _isatty(_fileno(stdout)) != 0;
#else
        return isatty(STDIN_FILENO) != 0 && isatty(STDOUT_FILENO) != 0;
#endif
    }

    /// The texts the menu offers: whatever `--text` named, then the three
    /// bundled corpora that are actually there.
    ///
    /// Existence is checked here rather than in the menu, so an installation
    /// missing its assets offers a short list instead of three entries that
    /// fail the moment they are chosen.
    std::vector<typeit::tui::TextChoice> bundled_texts(const typeit::cli::CliOptions& options,
                                                       const std::filesystem::path& assets) {
        std::vector<typeit::tui::TextChoice> texts;
        if (options.text_path.has_value()) {
            const std::filesystem::path given{*options.text_path};
            texts.push_back({.name = given.stem().string(), .path = given});
        }
        for (const std::string_view name: {"simple", "medium", "hard"}) {
            std::filesystem::path path = assets / "texts" / (std::string{name} + ".txt");
            std::error_code failed;
            if (std::filesystem::exists(path, failed)) {
                texts.push_back({.name = std::string{name}, .path = std::move(path)});
            }
        }
        return texts;
    }

    /// The terminal application: everything constructed, then handed over.
    int run_terminal(const typeit::cli::CliOptions& options, const typeit::infra::Environment& environment) {
        if (!has_a_terminal()) {
            std::cerr << "typeit: this needs a terminal. Try --simulate, --stats or --export.\n";
            return kFailed;
        }

        const Result<std::filesystem::path> directory = data_directory(options, environment);
        if (!directory) {
            return complain(directory.error());
        }
        const Result<std::unique_ptr<History>> history = open_history(*directory);
        if (!history) {
            return complain(history.error());
        }

        // A configuration file that will not parse is reported and the defaults
        // are used — a broken config is not a reason to be unusable
        // (ARCHITECTURE section 5.1).
        typeit::core::Config config;
        const Result<typeit::infra::Paths> paths = typeit::infra::resolve_paths(environment);
        if (paths) {
            typeit::infra::TomlConfigStore store{paths->config / "config.toml"};
            if (const Result<typeit::app::LoadedConfig> loaded = store.load(); loaded) {
                config = loaded->config;
                for (const std::string& warning: loaded->warnings) {
                    std::cerr << "typeit: config: " << warning << '\n';
                }
            } else {
                std::cerr << "typeit: config: " << typeit::core::to_string(loaded.error()) << '\n';
            }
        }

        // The theme, falling back to the built-in default with a word about it
        // rather than refusing to start over a colour.
        typeit::app::Theme theme;
        const Result<std::filesystem::path> assets = typeit::infra::locate_assets(typeit::infra::AssetSearch{
                .environment = environment,
                .executable = typeit::infra::current_executable().value_or(std::filesystem::path{}),
                .working_directory = std::filesystem::current_path(),
                .install_prefix = typeit::infra::configured_install_prefix(),
        });
        if (const Result<typeit::infra::LoadedTheme> loaded = typeit::infra::find_theme(
                    config.appearance.theme, paths ? paths->config / "themes" : std::filesystem::path{},
                    assets.value_or(std::filesystem::path{}) / "themes");
            loaded) {
            theme = loaded->theme;
        } else {
            std::cerr << "typeit: theme: " << typeit::core::to_string(loaded.error())
                      << "; using the built-in default\n";
        }

        const typeit::core::ModeRegistry modes = built_in_modes(config);
        const typeit::infra::SystemClock clock;
        const typeit::app::SessionService sessions{(*history)->repository, modes, clock, clock};

        typeit::tui::TerminalApp app{typeit::tui::Dependencies{
                .sessions = &sessions,
                .config = &config,
                .theme = &theme,
                .clock = &clock,
                .capabilities = typeit::infra::detect_capabilities(environment),
                .texts = bundled_texts(options, assets.value_or(std::filesystem::path{})),
                .load_text = read_file,
                // Only reached when nothing was found to offer, which means an
                // installation missing its assets. A sentence to type is better
                // than an empty screen; Phase 6's library replaces all of this.
                .text = "the quick brown fox jumps over the lazy dog",
        }};
        app.run();
        return kOk;
    }

    int not_yet(std::string_view what) {
        std::cerr << "typeit: " << what << " is not built yet.\n";
        return kFailed;
    }

    int dispatch(const typeit::cli::CliOptions& options) {
        const typeit::infra::Environment environment = typeit::infra::system_environment();

        switch (options.action) {
            case typeit::cli::Action::Help:
                return print(typeit::cli::usage());
            case typeit::cli::Action::Version:
                return print(typeit::cli::version_text());
            case typeit::cli::Action::Doctor:
                return run_doctor(environment);
            case typeit::cli::Action::Stats:
            case typeit::cli::Action::Export:
                return run_history_action(options, environment);
            case typeit::cli::Action::Simulate:
                return run_simulate(options, environment);
            case typeit::cli::Action::Run:
                return run_terminal(options, environment);
            case typeit::cli::Action::Import:
            case typeit::cli::Action::ImportDirectory:
            case typeit::cli::Action::ImportUrl:
            case typeit::cli::Action::ListTexts:
            case typeit::cli::Action::RemoveText:
                return not_yet("the text library");
        }
        return kFailed;
    }

}  // namespace

int main(int argc, char** argv) {
    // A closed pipe must be an error the writer sees, not a signal that kills
    // the process before it can report anything: `typeit --export json | head`
    // is an ordinary thing to type.
#ifdef SIGPIPE
    // NOLINTNEXTLINE(cert-err33-c) -- the previous handler is of no interest
    std::signal(SIGPIPE, SIG_IGN);
#endif

    try {
        // argv[0] is the program's own name, which the parser has no use for.
        // The pointer arithmetic is C's argv contract and there is no other way
        // in: `std::span{argv, argc}` would be the same arithmetic wearing a
        // hat.
        // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-pointer-arithmetic) -- argv is a [first, last)
        const std::vector<std::string_view> arguments{argv + 1, argv + argc};

        const Result<typeit::cli::CliOptions> options = typeit::cli::parse(arguments);
        if (!options) {
            std::cerr << "typeit: " << typeit::core::to_string(options.error()) << '\n';
            std::cerr << "Try 'typeit --help'.\n";
            return kMisused;
        }
        return dispatch(*options);
    } catch (const std::exception& thrown) {
        // The one top-level handler (ARCHITECTURE §6.3). Nothing in this
        // program throws across a layer boundary, so reaching here is a bug —
        // and a diagnostic is still better than a core dump.
        std::cerr << "typeit: unexpected failure: " << thrown.what() << '\n';
        return kFailed;
    } catch (...) {
        std::cerr << "typeit: unexpected failure\n";
        return kFailed;
    }
}
