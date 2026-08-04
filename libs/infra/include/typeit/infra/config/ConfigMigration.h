// Renamed and re-meaninged configuration keys (VERSIONING section 5).
//
// A settings file outlives the program that wrote it. When a key is renamed,
// the old file still has the old name in it, and the person who set that value
// meant it — dropping it silently and falling back to a default is the version
// of "your settings were lost" that nobody reports as a bug because they assume
// they imagined it.
//
// So: `config_version` counts up by one whenever a key is renamed or changes
// meaning, each step is a function from the old shape to the new one, and the
// file is backed up before it is rewritten.
#ifndef TYPEIT_INFRA_CONFIG_CONFIGMIGRATION_H
#define TYPEIT_INFRA_CONFIG_CONFIGMIGRATION_H

#include <cstdint>
#include <filesystem>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "typeit/core/util/Result.h"

namespace typeit::infra {

    /// One renamed key. The migration is textual rather than
    /// parse-and-rewrite: a config file is a document a person wrote, with
    /// their comments and their ordering in it, and round-tripping it through a
    /// TOML tree would hand it back sorted and stripped of every comment.
    struct ConfigRename {
        /// The `config_version` this step produces.
        std::int64_t to = 0;
        /// For the note the user is shown; the rename itself matches on the
        /// key, since a key name is unique across the file in practice.
        std::string_view section;
        std::string_view was;
        std::string_view now;
    };

    /// Every rename this binary knows, in ascending order.
    ///
    /// **Empty at 2.0.0**, and that is the honest state: nothing has been
    /// renamed yet. The mechanism ships anyway, because the first rename must
    /// not also be the release where the mechanism is written — that is the
    /// release where somebody's settings quietly disappear.
    [[nodiscard]] std::span<const ConfigRename> config_renames();

    /// The `config_version` this binary writes. Ships at 1.
    [[nodiscard]] std::int64_t latest_config_version();

    struct ConfigMigrationOutcome {
        std::int64_t from = 1;
        std::int64_t to = 1;
        /// What was renamed, in the user's words: `"[typing] stop_on_error was
        /// stop_at_error"`. Shown once, so a changed key is a thing that
        /// happened rather than a thing that vanished.
        std::vector<std::string> notes;
        /// Where the previous file was kept, when anything was changed.
        std::filesystem::path backup;
    };

    /// Rewrites `file` in place if it is older than this binary's version,
    /// having first copied it to `<file>.bak`.
    ///
    /// A file from the *future* is left completely alone and reported as a
    /// note, not an error: a newer TypeIt wrote it, the unknown keys will warn
    /// on load, and what this build does understand still works. Refusing to
    /// start because the settings are too new would be worse than starting with
    /// some of them ignored.
    [[nodiscard]] core::Result<ConfigMigrationOutcome> migrate_config_file(const std::filesystem::path& file);

    /// The same, against a rename table the caller supplies. Exists so the
    /// machinery can be tested while the real table is still empty: a migration
    /// nobody has run is a migration nobody knows works, and the first person
    /// to find out should not be a user whose settings are on the line.
    [[nodiscard]] core::Result<ConfigMigrationOutcome> migrate_config_file(const std::filesystem::path& file,
                                                                           std::span<const ConfigRename> renames,
                                                                           std::int64_t target_version);

}  // namespace typeit::infra

#endif  // TYPEIT_INFRA_CONFIG_CONFIGMIGRATION_H
