#include "typeit/infra/Doctor.h"

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <string>
#include <system_error>
#include <utility>
#include <vector>

#include "typeit/core/Version.h"
#include "typeit/core/util/Result.h"
#include "typeit/infra/Dependencies.h"
#include "typeit/infra/db/Migrator.h"
#include "typeit/infra/db/Schema.h"
#include "typeit/infra/db/SqliteDatabase.h"
#include "typeit/infra/fs/AssetLocator.h"
#include "typeit/infra/fs/PlatformPaths.h"
#include "typeit/infra/term/Capabilities.h"

namespace typeit::infra {
    namespace {

        /// Whether this program can write in `directory`, established by doing
        /// it. The permission bits are not the question: a directory can be
        /// mode 755 and still refuse a write over NFS, inside a container, or
        /// on a full disk.
        bool can_write_in(const std::filesystem::path& directory) {
            std::error_code failed;
            if (!std::filesystem::is_directory(directory, failed)) {
                return false;
            }
            // A name nothing else uses, removed whether or not the write
            // succeeded — a diagnostic that leaves litter behind is a
            // diagnostic that gets a bug report of its own.
            const std::filesystem::path probe = directory / ".typeit-doctor-probe";
            {
                const std::ofstream file{probe};
                if (!file) {
                    return false;
                }
            }
            std::filesystem::remove(probe, failed);
            return true;
        }

        PathReport report_on(std::string label, const std::filesystem::path& path) {
            std::error_code failed;
            const bool exists = std::filesystem::exists(path, failed);
            return PathReport{
                    .label = std::move(label),
                    .path = path,
                    .exists = exists,
                    .writable = exists && can_write_in(path),
                    .problem = {},
            };
        }

        PathReport missing(std::string label, std::string problem) {
            return PathReport{.label = std::move(label),
                              .path = {},
                              .exists = false,
                              .writable = false,
                              .problem = std::move(problem)};
        }

        /// `PRAGMA integrity_check` answers `ok` on one row when all is well,
        /// and a list of complaints when it is not. Only the first is worth
        /// printing: a corrupt file produces pages of them and the first says
        /// what happened.
        std::string integrity_of(SqliteDatabase& database) {
            core::Result<Statement> statement = database.prepare("PRAGMA integrity_check");
            if (!statement) {
                return statement.error().context.empty() ? statement.error().message : statement.error().context;
            }
            const core::Result<bool> row = statement->step();
            if (!row) {
                return row.error().context.empty() ? row.error().message : row.error().context;
            }
            if (!*row) {
                return "the integrity check answered nothing";
            }
            return statement->column_text(0);
        }

        /// A count, or zero when the table cannot be read. A database that is
        /// too broken to count rows in has already said so under `integrity`.
        std::int64_t count_or_zero(SqliteDatabase& database, std::string_view sql) {
            const core::Result<std::int64_t> count = database.query_int(sql);
            return count.value_or(0);
        }

        DatabaseReport examine_database(const std::filesystem::path& path) {
            DatabaseReport report;
            report.path = path;
            report.understood_version = latest_schema_version();

            std::error_code failed;
            report.exists = std::filesystem::exists(path, failed);
            if (!report.exists) {
                // Not a fault. A first run has no database, and saying "missing"
                // is more use than an error about a file nobody has made yet.
                report.integrity = "no database yet";
                return report;
            }

            // Opened rather than created: `--doctor` on a typo in `--data-dir`
            // must not leave an empty database behind to confuse the next run.
            core::Result<SqliteDatabase> database = SqliteDatabase::open(path, SqliteDatabase::OpenMode::OpenExisting);
            if (!database) {
                report.integrity =
                        database.error().context.empty() ? database.error().message : database.error().context;
                return report;
            }

            if (const core::Result<int> version = schema_version(*database); version) {
                report.schema_version = *version;
            }

            report.integrity = integrity_of(*database);
            report.sessions = count_or_zero(*database, "SELECT COUNT(*) FROM session");
            report.texts = count_or_zero(*database, "SELECT COUNT(*) FROM text_item");
            return report;
        }

        void append_line(std::string& out, std::string_view key, std::string_view value) {
            out += key;
            out += ": ";
            out += value;
            out += '\n';
        }

    }  // namespace

    DoctorReport diagnose(const Examination& examination) {
        DoctorReport report;
        report.version = kVersionString;
        report.git_describe = kGitDescribe;
        report.capabilities = detect_capabilities(examination.environment);
        report.sqlite_version = sqlite_version();
        report.toml_version = toml_version();

        const core::Result<Paths> paths = resolve_paths(examination.environment);
        if (paths) {
            report.paths.push_back(report_on("config", paths->config));
            report.paths.push_back(report_on("data", paths->data));
            report.paths.push_back(report_on("cache", paths->cache));
            report.database = examine_database(paths->data / "typeit.db");
        } else {
            const std::string why = paths.error().context.empty() ? paths.error().message : paths.error().context;
            for (const std::string label: {"config", "data", "cache"}) {
                report.paths.push_back(missing(label, why));
            }
            report.database.integrity = why;
            report.database.understood_version = latest_schema_version();
        }

        const core::Result<std::filesystem::path> assets = locate_assets(AssetSearch{
                .environment = examination.environment,
                .executable = examination.executable,
                .working_directory = examination.working_directory,
                .install_prefix = examination.install_prefix,
        });
        if (assets) {
            report.paths.push_back(report_on("assets", *assets));
        } else {
            report.paths.push_back(missing(
                    "assets", assets.error().context.empty() ? assets.error().message : assets.error().context));
        }

        return report;
    }

    std::string to_text(const DoctorReport& report) {
        std::string out;
        append_line(out, "typeit", report.version);
        append_line(out, "git", report.git_describe.empty() ? "unknown" : report.git_describe);

        out += "\n[terminal]\n";
        append_line(out, "color", to_string(report.capabilities.color));
        append_line(out, "glyphs", to_string(report.capabilities.glyphs));
        append_line(out, "reason", report.capabilities.reason);

        out += "\n[paths]\n";
        for (const PathReport& path: report.paths) {
            if (!path.problem.empty()) {
                append_line(out, path.label, "not found (" + path.problem + ")");
                continue;
            }
            std::string state = path.exists ? "exists" : "missing";
            state += path.writable ? ", writable" : ", not writable";
            append_line(out, path.label, path.path.string() + " (" + state + ")");
        }

        out += "\n[database]\n";
        append_line(out, "path", report.database.path.string());
        append_line(out, "schema",
                    report.database.schema_version.has_value() ? std::to_string(*report.database.schema_version)
                                                               : "unknown");
        append_line(out, "understood", std::to_string(report.database.understood_version));
        append_line(out, "sessions", std::to_string(report.database.sessions));
        append_line(out, "texts", std::to_string(report.database.texts));
        append_line(out, "integrity", report.database.integrity);

        out += "\n[dependencies]\n";
        append_line(out, "sqlite", report.sqlite_version);
        append_line(out, "toml++", report.toml_version);
        return out;
    }

}  // namespace typeit::infra
