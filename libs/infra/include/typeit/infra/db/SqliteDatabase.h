// One SQLite connection, its prepared statements, and a transaction guard
// (TECHNICAL section 4.2).
//
// Nothing above this layer knows SQLite exists; the two C types are forward
// declared so that this header pulls in no sqlite3.h and a caller cannot
// accidentally start using the C API through us.
//
// The rules this type exists to enforce:
//   * every statement is prepared once and reused,
//   * every variable is a bound parameter — there is no way to build SQL by
//     concatenation through this interface, and a lint test checks that nobody
//     did it another way,
//   * every write happens inside a transaction that rolls back unless it is
//     committed, including when a scope is left by an exception.
//
// Thread affinity: one connection, one thread (ARCHITECTURE section 6.4).
#ifndef TYPEIT_INFRA_DB_SQLITEDATABASE_H
#define TYPEIT_INFRA_DB_SQLITEDATABASE_H

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <string_view>

#include "typeit/core/util/Result.h"

struct sqlite3;
struct sqlite3_stmt;

namespace typeit::infra {

    class SqliteDatabase;

    /// A prepared statement, borrowed from the connection's cache.
    ///
    /// Binding is one-based, as SQLite's own API is: `?1` is index 1. Reading a
    /// column is zero-based, also as SQLite's is. The asymmetry is unpleasant
    /// and it is SQLite's; hiding it would mean every reader who knows SQL has
    /// to learn ours instead.
    ///
    /// Resets itself when it goes out of scope, so the cached statement is
    /// always clean for the next caller.
    class Statement {
    public:
        Statement() = default;
        ~Statement();
        Statement(const Statement&) = delete;
        Statement& operator=(const Statement&) = delete;
        Statement(Statement&& other) noexcept;
        Statement& operator=(Statement&& other) noexcept;

        Statement& bind(int index, std::int64_t value);
        Statement& bind(int index, double value);
        Statement& bind(int index, std::string_view value);
        Statement& bind_null(int index);

        /// True when a row is available, false when the statement is done.
        [[nodiscard]] core::Result<bool> step();

        /// Runs a statement that returns no rows. Fails if one does, because a
        /// caller ignoring returned rows is a caller who has misread their own
        /// query.
        [[nodiscard]] core::Status run();

        [[nodiscard]] std::int64_t column_int(int index) const;
        [[nodiscard]] double column_double(int index) const;
        [[nodiscard]] std::string column_text(int index) const;
        [[nodiscard]] bool column_is_null(int index) const;
        [[nodiscard]] int column_count() const;

        /// Clears the bindings and rewinds. Called for you on destruction.
        void reset();

    private:
        friend class SqliteDatabase;
        Statement(sqlite3* connection, sqlite3_stmt* statement) : connection_{connection}, statement_{statement} {}

        sqlite3* connection_ = nullptr;
        sqlite3_stmt* statement_ = nullptr;
    };

    /// BEGIN on construction; ROLLBACK on destruction unless committed.
    ///
    /// Deliberately not nestable. SQLite has savepoints, but every write path
    /// in this application is one transaction deep, and a nesting scheme
    /// nobody needs is a way to commit half a session without noticing.
    class Transaction {
    public:
        ~Transaction();
        Transaction(const Transaction&) = delete;
        Transaction& operator=(const Transaction&) = delete;
        Transaction(Transaction&& other) noexcept;
        Transaction& operator=(Transaction&& other) noexcept;

        /// After this, the destructor does nothing.
        [[nodiscard]] core::Status commit();

        [[nodiscard]] bool is_open() const noexcept { return database_ != nullptr; }

    private:
        friend class SqliteDatabase;
        explicit Transaction(SqliteDatabase* database) : database_{database} {}

        SqliteDatabase* database_ = nullptr;
    };

    class SqliteDatabase {
    public:
        enum class OpenMode : std::uint8_t {
            /// Fails when the file is not there. What a repair or an inspection
            /// tool wants: creating an empty database because of a typo in a
            /// path is not a service to anybody.
            OpenExisting,
            CreateIfMissing,
        };

        [[nodiscard]] static core::Result<SqliteDatabase> open(const std::filesystem::path& path, OpenMode mode);

        /// A private database that never touches a disk. Every test uses one
        /// unless it is specifically testing files — mocking SQL would test
        /// nothing, and the SQL is the part most likely to be wrong.
        [[nodiscard]] static core::Result<SqliteDatabase> open_in_memory();

        ~SqliteDatabase();
        SqliteDatabase(const SqliteDatabase&) = delete;
        SqliteDatabase& operator=(const SqliteDatabase&) = delete;
        SqliteDatabase(SqliteDatabase&& other) noexcept;
        SqliteDatabase& operator=(SqliteDatabase&& other) noexcept;

        /// Prepares `sql` the first time it is seen and reuses it afterwards.
        /// The statement text is the cache key, which is only sound because no
        /// query here is ever built by concatenation.
        [[nodiscard]] core::Result<Statement> prepare(std::string_view sql);

        /// For statements with no parameters: DDL, pragmas, and the migration
        /// scripts. Not a back door for interpolated SQL — it takes no values,
        /// so there is nothing to interpolate.
        [[nodiscard]] core::Status execute(std::string_view sql);

        /// Runs a script of several statements, one after another, without
        /// caching any of them. For migrations only: a schema file is many
        /// statements and is run once in the life of a database, so caching it
        /// would hold prepared DDL for the rest of the process.
        [[nodiscard]] core::Status execute_script(std::string_view sql);

        [[nodiscard]] core::Result<Transaction> begin();

        /// One integer from a one-column, one-row query. `PRAGMA user_version`
        /// and `SELECT COUNT(*)` are most of the reads in this codebase.
        [[nodiscard]] core::Result<std::int64_t> query_int(std::string_view sql);

        /// How many statements have actually been handed to SQLite for
        /// preparation. The cache is a claim about performance, and a claim
        /// about performance that nothing measures stops being true.
        [[nodiscard]] std::size_t prepared_count() const noexcept { return prepared_count_; }

        [[nodiscard]] std::size_t cached_count() const noexcept { return cache_.size(); }

        [[nodiscard]] bool in_transaction() const noexcept { return in_transaction_; }

        [[nodiscard]] const std::filesystem::path& path() const noexcept { return path_; }

    private:
        friend class Transaction;

        struct StatementDeleter {
            void operator()(sqlite3_stmt* statement) const noexcept;
        };
        struct ConnectionDeleter {
            void operator()(sqlite3* connection) const noexcept;
        };

        SqliteDatabase(sqlite3* connection, std::filesystem::path path);

        [[nodiscard]] core::Status apply_pragmas();
        void end_transaction(bool committed);

        std::unique_ptr<sqlite3, ConnectionDeleter> connection_;
        std::map<std::string, std::unique_ptr<sqlite3_stmt, StatementDeleter>, std::less<>> cache_;
        std::filesystem::path path_;
        std::size_t prepared_count_ = 0;
        bool in_transaction_ = false;
    };

}  // namespace typeit::infra

#endif  // TYPEIT_INFRA_DB_SQLITEDATABASE_H
