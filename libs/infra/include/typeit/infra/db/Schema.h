// The schema, as the binary carries it (TECHNICAL section 4.2).
//
// Compiled in rather than read from disk: a migration that depends on a file
// being installed correctly is a migration that fails on exactly the machines
// where recovering is hardest.
#ifndef TYPEIT_INFRA_DB_SCHEMA_H
#define TYPEIT_INFRA_DB_SCHEMA_H

#include <span>
#include <string_view>

namespace typeit::infra {

    struct Migration {
        /// The `user_version` the database has once this has been applied.
        /// Taken from the filename, so the number in `002_something.sql` is the
        /// number, and there is nothing to keep in sync.
        int version = 0;
        std::string_view name;
        std::string_view sql;
        /// `PRAGMA user_version = N`, written out at build time. SQLite parses
        /// pragma arguments when it prepares the statement, so the value cannot
        /// be a bound parameter — and generating the statement here means a
        /// version this binary does not know cannot be stamped at all.
        std::string_view stamp;
    };

    /// Every migration this binary knows, in ascending version order.
    [[nodiscard]] std::span<const Migration> migrations();

    /// The version a fully migrated database ends up at.
    [[nodiscard]] int latest_schema_version();

}  // namespace typeit::infra

#endif  // TYPEIT_INFRA_DB_SCHEMA_H
