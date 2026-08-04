// Where this program is allowed to put things (TECHNICAL section 4.1).
//
// XDG on Linux, %APPDATA%/%LOCALAPPDATA% on Windows, and `TYPEIT_CONFIG_DIR` /
// `TYPEIT_DATA_DIR` overriding both. The overrides are not a convenience: they
// are what makes every integration test from here on hermetic, so that no test
// can reach a developer's real history however badly it is written.
//
// The environment is a parameter, not a global read. A test that has to set a
// real environment variable to check a rule is a test that races every other
// test in the process.
#ifndef TYPEIT_INFRA_FS_PLATFORMPATHS_H
#define TYPEIT_INFRA_FS_PLATFORMPATHS_H

#include <filesystem>
#include <functional>
#include <optional>
#include <string>
#include <string_view>

#include "typeit/core/util/Result.h"

namespace typeit::infra {

    /// Reads one environment variable. Empty and unset are the same thing:
    /// `XDG_CONFIG_HOME=` is how a shell says "I have no opinion", and the
    /// specification says to fall back in exactly that case.
    using Environment = std::function<std::optional<std::string>(std::string_view)>;

    /// The real process environment. Used by `main`; never by a test.
    [[nodiscard]] Environment system_environment();

    struct Paths {
        /// `config.toml`, themes.
        std::filesystem::path config;
        /// The history database and imported texts.
        std::filesystem::path data;
        /// Logs and anything else that may be deleted without loss.
        std::filesystem::path cache;
    };

    /// Fails when the platform gives nothing to build a path from — no `HOME`
    /// on Linux, no `%APPDATA%` on Windows — rather than inventing a location
    /// and writing a user's history somewhere they will never find it.
    ///
    /// A relative `XDG_*` value is rejected: the specification says such a
    /// value "must be ignored", and ignoring it silently would put the database
    /// wherever the program happened to be started from.
    [[nodiscard]] core::Result<Paths> resolve_paths(const Environment& environment);

    /// Creates `directory` and its parents. Succeeding when it already exists
    /// is the point — this runs on every start.
    [[nodiscard]] core::Status ensure_directory(const std::filesystem::path& directory);

}  // namespace typeit::infra

#endif  // TYPEIT_INFRA_FS_PLATFORMPATHS_H
