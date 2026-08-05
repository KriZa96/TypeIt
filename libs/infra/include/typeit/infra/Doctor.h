// What to ask a user to run first (TI-076).
//
// Terminal capabilities, resolved paths, database health, and what this binary
// was linked against — the four things every "it looks wrong on my machine"
// report needs and never contains.
//
// It lives in `infra` because every fact in it *is* an infra fact: which
// environment variable decided the colour depth, whether a directory is
// writable, what SQLite says about the file. Routing that through ports so a
// higher layer could format it would mean three new interfaces with one
// implementation each, for a diagnostic nobody unit-tests through a fake.
//
// Nothing here throws and nothing here fails: a missing database, an
// unwritable directory and an unreadable file are *findings*, not errors. A
// diagnostic that refuses to run because something is wrong is a diagnostic
// that is never there when it is needed.
#ifndef TYPEIT_INFRA_DOCTOR_H
#define TYPEIT_INFRA_DOCTOR_H

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

#include "typeit/infra/fs/PlatformPaths.h"
#include "typeit/infra/term/Capabilities.h"

namespace typeit::infra {

    struct PathReport {
        /// `config`, `data`, `cache`, `assets`.
        std::string label;
        std::filesystem::path path;
        bool exists = false;
        /// Established by writing a file and removing it, not by reading the
        /// permission bits: a directory can be mode 755 and still refuse a
        /// write over NFS, inside a container, or on a full disk. The question
        /// is whether this program can write there, so the answer is to try.
        bool writable = false;
        /// Why there is no path at all — `TYPEIT_ASSETS_DIR` empty and nothing
        /// installed. Empty when the path was resolved.
        std::string problem;
    };

    struct DatabaseReport {
        std::filesystem::path path;
        bool exists = false;
        /// Absent when the database could not be opened or read at all.
        std::optional<int> schema_version;
        /// What this binary migrates to. A database ahead of it is refused at
        /// startup rather than upgraded, so the two numbers together are the
        /// whole answer to "why will it not open".
        int understood_version = 0;
        std::int64_t sessions = 0;
        std::int64_t texts = 0;
        /// `ok`, what SQLite said about a corrupt file, or why it could not be
        /// asked.
        std::string integrity;
    };

    struct DoctorReport {
        std::string version;
        std::string git_describe;
        Capabilities capabilities;
        std::vector<PathReport> paths;
        DatabaseReport database;
        std::string sqlite_version;
        std::string toml_version;
    };

    /// Everything the report depends on, passed in rather than looked up, so a
    /// test can lay out a broken install in a temp directory and ask the real
    /// question about it.
    struct Examination {
        Environment environment;
        /// The running binary, symlinks resolved, for the asset search.
        std::filesystem::path executable;
        std::filesystem::path working_directory;
        std::filesystem::path install_prefix;
    };

    [[nodiscard]] DoctorReport diagnose(const Examination& examination);

    /// The report as `key: value` lines under `[section]` headers, in a fixed
    /// order with nothing dated in it — so two runs on one machine differ only
    /// where the machine differs, and a user can paste it into an issue
    /// without a screenshot.
    [[nodiscard]] std::string to_text(const DoctorReport& report);

}  // namespace typeit::infra

#endif  // TYPEIT_INFRA_DOCTOR_H
