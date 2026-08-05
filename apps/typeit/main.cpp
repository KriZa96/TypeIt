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
// The terminal application is Phase 4. Until then the actions that need a
// screen say so plainly rather than doing nothing.

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
#include "typeit/infra/db/Migrator.h"
#include "typeit/infra/db/SqliteDatabase.h"
#include "typeit/infra/db/SqliteHistoryRepository.h"
#include "typeit/infra/fs/AssetLocator.h"
#include "typeit/infra/fs/PlatformPaths.h"
#include "typeit/infra/time/SystemClock.h"

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

    /// The modes a run can be started in, each closed over the configuration it
    /// needs. `ModeRegistry`'s factories take no arguments by design (ADR-008),
    /// so the parameter is baked in here, where the configuration is known.
    typeit::core::ModeRegistry built_in_modes(const typeit::core::Config& config) {
        typeit::core::ModeRegistry registry;
        registry.register_mode("timed", [duration = typeit::core::Millis{config.general.default_duration_s * 1'000}] {
            return std::make_unique<typeit::core::TimedMode>(duration);
        });
        registry.register_mode("words", [words = static_cast<std::size_t>(config.general.default_word_count)] {
            return std::make_unique<typeit::core::WordCountMode>(words);
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
                return not_yet("the terminal application");
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
