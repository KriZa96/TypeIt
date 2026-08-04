// Bringing a database up to the schema this binary understands
// (TECHNICAL section 4.2, VERSIONING section 5).
//
// Keyed on `PRAGMA user_version`, applied in order, each in its own
// transaction. A migration that fails leaves the database exactly as it was,
// at the version it was, so the next start tries the same step again rather
// than the next one.
//
// A database from a *newer* binary is refused outright. It is not upgraded, not
// downgraded, and above all not recreated: somebody's entire typing history is
// in there, and the only safe answer to "I do not understand this file" is to
// say so.
#ifndef TYPEIT_INFRA_DB_MIGRATOR_H
#define TYPEIT_INFRA_DB_MIGRATOR_H

#include "typeit/core/util/Result.h"
#include "typeit/infra/db/SqliteDatabase.h"

namespace typeit::infra {

    struct MigrationOutcome {
        int from = 0;
        int to = 0;
        /// How many migration scripts ran. Zero when the database was already
        /// current, which is the common case and not a failure.
        int applied = 0;
    };

    [[nodiscard]] core::Result<int> schema_version(SqliteDatabase& database);

    [[nodiscard]] core::Result<MigrationOutcome> migrate_to_latest(SqliteDatabase& database);

}  // namespace typeit::infra

#endif  // TYPEIT_INFRA_DB_MIGRATOR_H
