#include "typeit/infra/db/SqliteDatabase.h"

#include <cstdint>
#include <filesystem>
#include <memory>
#include <sqlite3.h>
#include <string>
#include <string_view>
#include <utility>

#include "typeit/core/util/Result.h"

namespace typeit::infra {
    namespace {

        using core::ErrorCode;
        using core::Result;
        using core::Status;

        /// SQLite's own message, which names the table, the column or the
        /// constraint. Anything we invented here would say less.
        std::string message_of(sqlite3* connection) {
            const char* message = sqlite3_errmsg(connection);
            return message == nullptr ? "unknown SQLite error" : message;
        }

        Status database_error(sqlite3* connection, std::string_view what) {
            return core::fail(ErrorCode::DbQuery, std::string{what} + ": " + message_of(connection));
        }

    }  // namespace

    // ---- Statement ---------------------------------------------------------

    void SqliteDatabase::StatementDeleter::operator()(sqlite3_stmt* statement) const noexcept {
        sqlite3_finalize(statement);
    }

    void SqliteDatabase::ConnectionDeleter::operator()(sqlite3* connection) const noexcept {
        sqlite3_close_v2(connection);
    }

    Statement::~Statement() { reset(); }

    Statement::Statement(Statement&& other) noexcept :
        connection_{std::exchange(other.connection_, nullptr)}, statement_{std::exchange(other.statement_, nullptr)} {}

    Statement& Statement::operator=(Statement&& other) noexcept {
        if (this != &other) {
            reset();
            connection_ = std::exchange(other.connection_, nullptr);
            statement_ = std::exchange(other.statement_, nullptr);
        }
        return *this;
    }

    void Statement::reset() {
        if (statement_ != nullptr) {
            sqlite3_reset(statement_);
            sqlite3_clear_bindings(statement_);
        }
    }

    Statement& Statement::bind(int index, std::int64_t value) {
        sqlite3_bind_int64(statement_, index, value);
        return *this;
    }

    Statement& Statement::bind(int index, double value) {
        sqlite3_bind_double(statement_, index, value);
        return *this;
    }

    Statement& Statement::bind(int index, std::string_view value) {
        // SQLITE_TRANSIENT: SQLite copies the bytes, so a caller may bind a
        // temporary without thinking about how long it lives. The alternative
        // is a lifetime rule nobody would remember at every call site.
        sqlite3_bind_text(statement_, index, value.data(), static_cast<int>(value.size()), SQLITE_TRANSIENT);
        return *this;
    }

    Statement& Statement::bind_null(int index) {
        sqlite3_bind_null(statement_, index);
        return *this;
    }

    Result<bool> Statement::step() {
        const int status = sqlite3_step(statement_);
        if (status == SQLITE_ROW) {
            return true;
        }
        if (status == SQLITE_DONE) {
            return false;
        }
        return std::unexpected{database_error(connection_, "step").error()};
    }

    Status Statement::run() {
        const Result<bool> row = step();
        if (!row) {
            return std::unexpected{row.error()};
        }
        if (*row) {
            return core::fail(ErrorCode::DbQuery, "this statement returned rows; use step() and read them");
        }
        return {};
    }

    std::int64_t Statement::column_int(int index) const { return sqlite3_column_int64(statement_, index); }

    double Statement::column_double(int index) const { return sqlite3_column_double(statement_, index); }

    std::string Statement::column_text(int index) const {
        const auto* text = sqlite3_column_text(statement_, index);
        if (text == nullptr) {
            return {};
        }
        const int size = sqlite3_column_bytes(statement_, index);
        // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast) -- SQLite hands back unsigned char
        return {reinterpret_cast<const char*>(text), static_cast<std::size_t>(size)};
    }

    bool Statement::column_is_null(int index) const { return sqlite3_column_type(statement_, index) == SQLITE_NULL; }

    int Statement::column_count() const { return sqlite3_column_count(statement_); }

    // ---- Transaction -------------------------------------------------------

    Transaction::~Transaction() {
        if (database_ != nullptr) {
            // Nothing was committed, so nothing happened. This is the path a
            // thrown exception takes, and the reason a half-written session
            // cannot reach the disk.
            database_->end_transaction(false);
        }
    }

    Transaction::Transaction(Transaction&& other) noexcept : database_{std::exchange(other.database_, nullptr)} {}

    Transaction& Transaction::operator=(Transaction&& other) noexcept {
        if (this != &other) {
            if (database_ != nullptr) {
                database_->end_transaction(false);
            }
            database_ = std::exchange(other.database_, nullptr);
        }
        return *this;
    }

    Status Transaction::commit() {
        if (database_ == nullptr) {
            return core::fail(ErrorCode::DbQuery, "this transaction has already ended");
        }
        SqliteDatabase* database = std::exchange(database_, nullptr);
        const Status committed = database->execute("COMMIT");
        database->in_transaction_ = false;
        return committed;
    }

    // ---- SqliteDatabase ----------------------------------------------------

    SqliteDatabase::SqliteDatabase(sqlite3* connection, std::filesystem::path path) :
        connection_{connection}, path_{std::move(path)} {}

    SqliteDatabase::~SqliteDatabase() = default;

    SqliteDatabase::SqliteDatabase(SqliteDatabase&& other) noexcept = default;

    SqliteDatabase& SqliteDatabase::operator=(SqliteDatabase&& other) noexcept = default;

    Result<SqliteDatabase> SqliteDatabase::open(const std::filesystem::path& path, OpenMode mode) {
        int flags = SQLITE_OPEN_READWRITE;
        if (mode == OpenMode::CreateIfMissing) {
            flags |= SQLITE_OPEN_CREATE;
        }

        sqlite3* connection = nullptr;
        const int status = sqlite3_open_v2(path.string().c_str(), &connection, flags, nullptr);
        if (status != SQLITE_OK) {
            // The handle is returned even on failure, and it carries the
            // message; closing it is ours to do either way.
            const std::string message = connection == nullptr ? sqlite3_errstr(status) : message_of(connection);
            sqlite3_close_v2(connection);
            return core::fail(ErrorCode::DbOpen, path.string() + ": " + message);
        }

        SqliteDatabase database{connection, path};
        if (const Status pragmas = database.apply_pragmas(); !pragmas) {
            return std::unexpected{pragmas.error()};
        }
        return database;
    }

    Result<SqliteDatabase> SqliteDatabase::open_in_memory() {
        return open(std::filesystem::path{":memory:"}, OpenMode::CreateIfMissing);
    }

    Status SqliteDatabase::apply_pragmas() {
        // TECHNICAL section 4.2, and each is load-bearing:
        //
        //   WAL          — a reader never blocks the writer, which is what lets
        //                  a second TypeIt look at the history while one types.
        //   foreign_keys — SQLite has them off by default, so every ON DELETE
        //                  CASCADE in the schema is decoration until this runs.
        //   NORMAL       — fsync at checkpoints rather than at every commit. A
        //                  power cut can lose the last session; it cannot
        //                  corrupt the database.
        //   busy_timeout — wait rather than fail when another process holds the
        //                  write lock.
        //
        // journal_mode returns a row, so it is executed as a query.
        if (const Result<std::int64_t> wal_mode = query_int("PRAGMA journal_mode = WAL"); !wal_mode) {
            // An in-memory database refuses WAL and stays in "memory" mode,
            // which is correct and not an error.
            if (wal_mode.error().code != ErrorCode::DbQuery) {
                return std::unexpected{wal_mode.error()};
            }
        }

        for (const std::string_view pragma:
             {"PRAGMA foreign_keys = ON", "PRAGMA synchronous = NORMAL", "PRAGMA busy_timeout = 3000"}) {
            if (const Status applied = execute(pragma); !applied) {
                return applied;
            }
        }
        return {};
    }

    Result<Statement> SqliteDatabase::prepare(std::string_view sql) {
        if (const auto cached = cache_.find(sql); cached != cache_.end()) {
            return Statement{connection_.get(), cached->second.get()};
        }

        sqlite3_stmt* statement = nullptr;
        const int status =
                sqlite3_prepare_v2(connection_.get(), sql.data(), static_cast<int>(sql.size()), &statement, nullptr);
        if (status != SQLITE_OK) {
            return core::fail(ErrorCode::DbQuery, std::string{sql} + ": " + message_of(connection_.get()));
        }

        ++prepared_count_;
        const auto inserted =
                cache_.emplace(std::string{sql}, std::unique_ptr<sqlite3_stmt, StatementDeleter>{statement});
        return Statement{connection_.get(), inserted.first->second.get()};
    }

    Status SqliteDatabase::execute(std::string_view sql) {
        Result<Statement> statement = prepare(sql);
        if (!statement) {
            return std::unexpected{statement.error()};
        }
        const Result<bool> row = statement->step();
        if (!row) {
            return std::unexpected{row.error()};
        }
        return {};
    }

    Result<std::int64_t> SqliteDatabase::query_int(std::string_view sql) {
        Result<Statement> statement = prepare(sql);
        if (!statement) {
            return std::unexpected{statement.error()};
        }
        const Result<bool> row = statement->step();
        if (!row) {
            return std::unexpected{row.error()};
        }
        if (!*row) {
            return core::fail(ErrorCode::DbQuery, std::string{sql} + ": expected a row, got none");
        }
        return statement->column_int(0);
    }

    Result<Transaction> SqliteDatabase::begin() {
        if (in_transaction_) {
            // Deliberately refused rather than mapped to a savepoint: every
            // write path here is one transaction deep, and a nesting scheme
            // nobody needs is a way to commit half a session by accident.
            return core::fail(ErrorCode::DbQuery, "a transaction is already open on this connection");
        }
        if (const Status begun = execute("BEGIN"); !begun) {
            return std::unexpected{begun.error()};
        }
        in_transaction_ = true;
        return Transaction{this};
    }

    void SqliteDatabase::end_transaction(bool committed) {
        if (!in_transaction_) {
            return;
        }
        if (!committed) {
            // Nothing useful to do with a failure here: the caller is already
            // unwinding, and SQLite rolls back an open transaction when the
            // connection closes anyway. Assigning it makes that deliberate
            // rather than a discarded [[nodiscard]].
            const Status rolled_back = execute("ROLLBACK");
            static_cast<void>(rolled_back);
        }
        in_transaction_ = false;
    }

}  // namespace typeit::infra
