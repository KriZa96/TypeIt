#include "typeit/infra/config/ConfigMigration.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <span>
#include <sstream>
#include <string>
#include <string_view>
#include <system_error>
#include <vector>

#include "typeit/core/util/Result.h"

namespace typeit::infra {
    namespace {

        using core::ErrorCode;
        using core::Result;
        using core::Status;

        /// The `config_version` a file declares, or 1 when it says nothing:
        /// version 1 is the shape that had no version key yet, so a file
        /// without one is a version-1 file and not an error.
        std::int64_t declared_version(std::string_view text) {
            std::istringstream lines{std::string{text}};
            std::string line;
            while (std::getline(lines, line)) {
                const std::string_view view{line};
                const std::size_t equals = view.find('=');
                if (equals == std::string_view::npos) {
                    continue;
                }
                std::string_view key = view.substr(0, equals);
                while (!key.empty() && (key.front() == ' ' || key.front() == '\t')) {
                    key.remove_prefix(1);
                }
                while (!key.empty() && (key.back() == ' ' || key.back() == '\t')) {
                    key.remove_suffix(1);
                }
                if (key != "config_version") {
                    continue;
                }
                std::string_view value = view.substr(equals + 1);
                std::int64_t parsed = 0;
                bool digits = false;
                for (const char character: value) {
                    if (character >= '0' && character <= '9') {
                        parsed = (parsed * 10) + (character - '0');
                        digits = true;
                    } else if (digits) {
                        break;
                    }
                }
                return digits ? parsed : 1;
            }
            return 1;
        }

        Result<std::string> read_whole(const std::filesystem::path& file) {
            std::ifstream in{file, std::ios::binary};
            if (!in) {
                return core::fail(ErrorCode::FileNotFound, file.string());
            }
            std::ostringstream text;
            text << in.rdbuf();
            return text.str();
        }

        Status write_whole(const std::filesystem::path& file, std::string_view text) {
            // Same temp-and-rename as the config store: a crash mid-write must
            // not leave a half-migrated file where the settings used to be.
            const std::filesystem::path temporary = file.string() + ".tmp";
            {
                std::ofstream out{temporary, std::ios::binary | std::ios::trunc};
                if (!out) {
                    return core::fail(ErrorCode::FileUnreadable, temporary.string() + ": cannot write");
                }
                out << text;
                out.flush();
                if (!out) {
                    return core::fail(ErrorCode::FileUnreadable, temporary.string() + ": write failed");
                }
            }
            std::error_code failure;
            std::filesystem::rename(temporary, file, failure);
            if (failure) {
                std::filesystem::remove(temporary, failure);
                return core::fail(ErrorCode::FileUnreadable, file.string() + ": " + failure.message());
            }
            return {};
        }

        /// `key = value` becomes `new_key = value`, leaving the spacing and any
        /// trailing comment where they were.
        bool rename_key(std::string& text, std::string_view was, std::string_view now, std::string& note) {
            std::istringstream lines{text};
            std::ostringstream rewritten;
            std::string line;
            bool changed = false;

            while (std::getline(lines, line)) {
                const std::string_view view{line};
                const std::size_t start = std::min(view.find_first_not_of(" \t"), view.size());
                const std::size_t equals = view.find('=', start);
                if (equals != std::string_view::npos) {
                    std::string_view key = view.substr(start, equals - start);
                    while (!key.empty() && (key.back() == ' ' || key.back() == '\t')) {
                        key.remove_suffix(1);
                    }
                    if (key == was) {
                        rewritten << line.substr(0, start) << now << line.substr(start + key.size()) << "\n";
                        changed = true;
                        note = std::string{was} + " is now " + std::string{now};
                        continue;
                    }
                }
                rewritten << line << "\n";
            }

            if (changed) {
                text = rewritten.str();
            }
            return changed;
        }

        void set_version(std::string& text, std::int64_t version) {
            std::string note;
            // Rewriting the existing line keeps it where the user has it; a
            // file without one gets it at the top, where the template puts it.
            if (!rename_key(text, "config_version", "config_version", note)) {
                text = "config_version = " + std::to_string(version) + "\n" + text;
                return;
            }

            std::istringstream lines{text};
            std::ostringstream rewritten;
            std::string line;
            while (std::getline(lines, line)) {
                if (line.starts_with("config_version")) {
                    rewritten << "config_version = " << version << "\n";
                } else {
                    rewritten << line << "\n";
                }
            }
            text = rewritten.str();
        }

    }  // namespace

    std::span<const ConfigRename> config_renames() {
        static constexpr std::array<ConfigRename, 0> kRenames{};
        return kRenames;
    }

    std::int64_t latest_config_version() {
        std::int64_t latest = 1;
        for (const ConfigRename& rename: config_renames()) {
            latest = rename.to > latest ? rename.to : latest;
        }
        return latest;
    }

    Result<ConfigMigrationOutcome> migrate_config_file(const std::filesystem::path& file) {
        return migrate_config_file(file, config_renames(), latest_config_version());
    }

    Result<ConfigMigrationOutcome> migrate_config_file(const std::filesystem::path& file,
                                                       std::span<const ConfigRename> renames,
                                                       std::int64_t target_version) {
        std::error_code ignored;
        if (!std::filesystem::exists(file, ignored)) {
            // Nothing to migrate. A first run writes a current file.
            ConfigMigrationOutcome nothing_to_do;
            nothing_to_do.from = target_version;
            nothing_to_do.to = target_version;
            return nothing_to_do;
        }

        Result<std::string> text = read_whole(file);
        if (!text) {
            return std::unexpected{text.error()};
        }

        const std::int64_t from = declared_version(*text);
        ConfigMigrationOutcome outcome;
        outcome.from = from;
        outcome.to = from;

        if (from > target_version) {
            // Written by a newer TypeIt. Left completely alone: the unknown
            // keys warn on load, what this build understands still works, and
            // refusing to start would be worse than starting with some settings
            // ignored.
            outcome.notes.emplace_back("this configuration was written by a newer version of TypeIt (version " +
                                       std::to_string(from) + "); settings it does not share are left untouched");
            return outcome;
        }
        if (from == target_version) {
            return outcome;  // Current. The common case, and not a failure.
        }

        std::string migrated = *text;
        for (const ConfigRename& rename: renames) {
            if (rename.to <= from) {
                continue;
            }
            std::string note;
            if (rename_key(migrated, rename.was, rename.now, note)) {
                outcome.notes.push_back("[" + std::string{rename.section} + "] " + note);
            }
            outcome.to = rename.to;
        }
        outcome.to = target_version;
        set_version(migrated, outcome.to);

        // The backup comes first, always, and before anything is written. A
        // migration that goes wrong should cost nothing but a rename.
        outcome.backup = file.string() + ".bak";
        std::filesystem::copy_file(file, outcome.backup, std::filesystem::copy_options::overwrite_existing, ignored);
        if (ignored) {
            return core::fail(ErrorCode::FileUnreadable,
                              outcome.backup.string() + ": cannot write the backup, so nothing was migrated");
        }

        if (const Status written = write_whole(file, migrated); !written) {
            return std::unexpected{written.error()};
        }
        return outcome;
    }

}  // namespace typeit::infra
