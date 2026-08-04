#include "typeit/infra/db/Migrator.h"

#include <cstdint>
#include <span>
#include <string>

#include "typeit/core/util/Result.h"
#include "typeit/infra/db/Schema.h"
#include "typeit/infra/db/SqliteDatabase.h"

namespace typeit::infra {
    namespace {

        using core::ErrorCode;
        using core::Result;
        using core::Status;

        /// `PRAGMA user_version = ?` does not accept a bound parameter — SQLite
        /// parses pragma arguments at prepare time. So the value is written by
        /// selecting the statement from a fixed table rather than by building
        /// one, which keeps the no-concatenated-SQL rule intact and, more
        /// usefully, means a version this binary does not know cannot be
        /// written at all.
        Status stamp_version(SqliteDatabase& database, int version) {
            for (const Migration& migration: migrations()) {
                if (migration.version == version) {
                    return database.execute(migration.stamp);
                }
            }
            return core::fail(ErrorCode::DbMigrate, "no statement to stamp version " + std::to_string(version));
        }

    }  // namespace

    int latest_schema_version() {
        int latest = 0;
        for (const Migration& migration: migrations()) {
            latest = migration.version > latest ? migration.version : latest;
        }
        return latest;
    }

    Result<int> schema_version(SqliteDatabase& database) {
        const Result<std::int64_t> version = database.query_int("PRAGMA user_version");
        if (!version) {
            return std::unexpected{version.error()};
        }
        return static_cast<int>(*version);
    }

    Result<MigrationOutcome> migrate_to_latest(SqliteDatabase& database) {
        const Result<int> current = schema_version(database);
        if (!current) {
            return std::unexpected{current.error()};
        }

        const int latest = latest_schema_version();
        if (*current > latest) {
            // Somebody ran a newer TypeIt against this file. Their history is
            // in there; the only safe answer is to say so and stop.
            return core::fail(ErrorCode::DbMigrate, "this database was written by a newer version of TypeIt (schema " +
                                                            std::to_string(*current) + ", this build understands " +
                                                            std::to_string(latest) +
                                                            "). Upgrade TypeIt, or move the file aside.");
        }

        MigrationOutcome outcome{.from = *current, .to = *current, .applied = 0};
        for (const Migration& migration: migrations()) {
            if (migration.version <= outcome.to) {
                continue;
            }

            // One transaction per migration. A failure leaves the database at
            // the version it was, so the next start retries this step rather
            // than the one after it.
            Result<Transaction> transaction = database.begin();
            if (!transaction) {
                return std::unexpected{transaction.error()};
            }

            if (const Status applied = database.execute_script(migration.sql); !applied) {
                return core::fail(ErrorCode::DbMigrate, std::string{migration.name} + ": " + applied.error().context);
            }
            if (const Status stamped = stamp_version(database, migration.version); !stamped) {
                return core::fail(ErrorCode::DbMigrate, std::string{migration.name} + ": " + stamped.error().context);
            }
            if (const Status committed = transaction->commit(); !committed) {
                return core::fail(ErrorCode::DbMigrate, std::string{migration.name} + ": " + committed.error().context);
            }

            outcome.to = migration.version;
            ++outcome.applied;
        }

        return outcome;
    }

}  // namespace typeit::infra
