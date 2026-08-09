#include <cstdint>
#include <filesystem>
#include <fstream>
#include <gtest/gtest.h>
#include <string>
#include <utility>

#include "typeit/core/util/Result.h"
#include "typeit/infra/db/SqliteDatabase.h"
#include "typeit/testing/TempEnv.h"

namespace typeit::infra {
    namespace {

        using core::ErrorCode;
        using core::Result;
        using core::Status;

        SqliteDatabase in_memory() {
            Result<SqliteDatabase> database = SqliteDatabase::open_in_memory();
            EXPECT_TRUE(database) << (database ? "" : database.error().context);
            return std::move(*database);
        }

        // ---- the SQL this project needs to exist -----------------------------

        TEST(SqliteDatabaseTest, TheOptionalSqlFunctionsThisProjectUsesArePresent) {
            // `floor()` has been opt-in since SQLite 3.35, and the daily-totals
            // query needs it. The build prefers a system SQLite and falls back
            // to the amalgamation, so this is a property of whichever one was
            // found rather than of the source — which is exactly why it is
            // asserted here, at the seam, rather than left to surface as `no
            // such function: FLOOR` from inside a history query.
            //
            // That is not hypothetical: it is what every Windows build did,
            // unnoticed, for as long as the Windows job failed before its tests
            // ever ran.
            SqliteDatabase database = in_memory();

            const Result<std::int64_t> floored = database.query_int("SELECT CAST(FLOOR(-1.5) AS INTEGER)");

            ASSERT_TRUE(floored) << (floored ? "" : floored.error().context);
            EXPECT_EQ(*floored, -2) << "and it floors rather than truncating, which is the reason it is used";
        }

        /// A table to write into. Every test that needs one needs the same one.
        void make_table(SqliteDatabase& database) {
            ASSERT_TRUE(database.execute("CREATE TABLE note (id INTEGER PRIMARY KEY, body TEXT NOT NULL)"));
        }

        std::int64_t count_notes(SqliteDatabase& database) {
            const Result<std::int64_t> count = database.query_int("SELECT COUNT(*) FROM note");
            EXPECT_TRUE(count) << (count ? "" : count.error().context);
            return count.value_or(-1);
        }

        Status insert_note(SqliteDatabase& database, std::int64_t id, std::string_view body) {
            Result<Statement> statement = database.prepare("INSERT INTO note (id, body) VALUES (?1, ?2)");
            if (!statement) {
                return std::unexpected{statement.error()};
            }
            return statement->bind(1, id).bind(2, body).run();
        }

        // ---- opening -------------------------------------------------------

        TEST(SqliteDatabaseTest, AnInMemoryDatabaseOpens) {
            const Result<SqliteDatabase> database = SqliteDatabase::open_in_memory();

            ASSERT_TRUE(database) << (database ? "" : database.error().context);
            EXPECT_EQ(database->path(), std::filesystem::path{":memory:"});
        }

        TEST(SqliteDatabaseTest, CreateIfMissingMakesTheFile) {
            const testing::TempEnv env;
            const std::filesystem::path file = env.data() / "history.db";

            const Result<SqliteDatabase> database =
                    SqliteDatabase::open(file, SqliteDatabase::OpenMode::CreateIfMissing);

            ASSERT_TRUE(database) << (database ? "" : database.error().context);
            EXPECT_TRUE(std::filesystem::exists(file));
        }

        TEST(SqliteDatabaseTest, OpenExistingRefusesToInventADatabase) {
            // A typo in a path should not silently produce an empty history.
            const testing::TempEnv env;

            const Result<SqliteDatabase> database =
                    SqliteDatabase::open(env.data() / "not-here.db", SqliteDatabase::OpenMode::OpenExisting);

            ASSERT_FALSE(database);
            EXPECT_EQ(database.error().code, ErrorCode::DbOpen);
            EXPECT_NE(database.error().context.find("not-here.db"), std::string::npos) << database.error().context;
        }

        TEST(SqliteDatabaseTest, OpeningADirectoryIsAnError) {
            const testing::TempEnv env;

            const Result<SqliteDatabase> database =
                    SqliteDatabase::open(env.data(), SqliteDatabase::OpenMode::CreateIfMissing);

            ASSERT_FALSE(database);
            EXPECT_EQ(database.error().code, ErrorCode::DbOpen);
        }

        TEST(SqliteDatabaseTest, OpeningSomethingThatIsNotADatabaseFailsWhenItIsUsed) {
            // SQLite is lazy: the header is only read on first access, so this
            // is where the failure has to be reported clearly.
            const testing::TempEnv env;
            const std::filesystem::path file = env.data() / "notes.txt";
            std::ofstream{file} << "this is not a database, it is a shopping list";

            Result<SqliteDatabase> database = SqliteDatabase::open(file, SqliteDatabase::OpenMode::OpenExisting);
            const Status used = database ? database->execute("CREATE TABLE t (x INTEGER)") : Status{};

            EXPECT_FALSE(database && used);
        }

        // ---- pragmas -------------------------------------------------------

        TEST(SqliteDatabaseTest, EveryDocumentedPragmaIsActuallyApplied) {
            // Read back rather than assumed. foreign_keys in particular is off
            // by default, which would make every ON DELETE CASCADE in the
            // schema decoration.
            const testing::TempEnv env;
            Result<SqliteDatabase> database =
                    SqliteDatabase::open(env.data() / "pragmas.db", SqliteDatabase::OpenMode::CreateIfMissing);
            ASSERT_TRUE(database) << (database ? "" : database.error().context);

            EXPECT_EQ(database->query_int("PRAGMA foreign_keys").value_or(-1), 1);
            EXPECT_EQ(database->query_int("PRAGMA synchronous").value_or(-1), 1) << "NORMAL";
            EXPECT_EQ(database->query_int("PRAGMA busy_timeout").value_or(-1), 3'000);

            Result<Statement> journal = database->prepare("PRAGMA journal_mode");
            ASSERT_TRUE(journal);
            ASSERT_TRUE(journal->step().value_or(false));
            EXPECT_EQ(journal->column_text(0), "wal");
        }

        TEST(SqliteDatabaseTest, AnInMemoryDatabaseKeepsTheOtherPragmasWithoutWal) {
            // WAL needs a file. Refusing it is correct, and must not stop the
            // connection from opening — every test in this suite depends on it.
            SqliteDatabase database = in_memory();

            EXPECT_EQ(database.query_int("PRAGMA foreign_keys").value_or(-1), 1);
            EXPECT_EQ(database.query_int("PRAGMA busy_timeout").value_or(-1), 3'000);
        }

        TEST(SqliteDatabaseTest, ForeignKeysAreEnforcedNotJustEnabled) {
            SqliteDatabase database = in_memory();
            ASSERT_TRUE(database.execute("CREATE TABLE parent (id INTEGER PRIMARY KEY)"));
            ASSERT_TRUE(database.execute(
                    "CREATE TABLE child (id INTEGER PRIMARY KEY, parent_id INTEGER REFERENCES parent(id))"));

            Result<Statement> orphan = database.prepare("INSERT INTO child (id, parent_id) VALUES (1, 99)");
            ASSERT_TRUE(orphan);

            const Status inserted = orphan->run();

            ASSERT_FALSE(inserted);
            EXPECT_NE(inserted.error().context.find("FOREIGN KEY"), std::string::npos) << inserted.error().context;
        }

        // ---- statements ----------------------------------------------------

        TEST(SqliteDatabaseTest, APreparedStatementIsReusedRatherThanRePrepared) {
            // The cache is a claim about performance, and one nothing measures
            // stops being true.
            SqliteDatabase database = in_memory();
            make_table(database);
            const std::size_t after_ddl = database.prepared_count();

            for (std::int64_t id = 1; id <= 100; ++id) {
                ASSERT_TRUE(insert_note(database, id, "body"));
            }

            EXPECT_EQ(database.prepared_count(), after_ddl + 1) << "one INSERT, prepared once";
            EXPECT_EQ(count_notes(database), 100);
        }

        TEST(SqliteDatabaseTest, ADifferentStatementIsPreparedSeparately) {
            SqliteDatabase database = in_memory();
            make_table(database);
            const std::size_t before = database.prepared_count();

            ASSERT_TRUE(insert_note(database, 1, "one"));
            static_cast<void>(count_notes(database));

            EXPECT_EQ(database.prepared_count(), before + 2);
        }

        TEST(SqliteDatabaseTest, ACachedStatementComesBackClean) {
            // The bindings of the previous caller must not leak into the next
            // one, or a partially bound statement silently reuses stale values.
            SqliteDatabase database = in_memory();
            make_table(database);
            ASSERT_TRUE(insert_note(database, 1, "first"));

            {
                Result<Statement> statement = database.prepare("INSERT INTO note (id, body) VALUES (?1, ?2)");
                ASSERT_TRUE(statement);
                statement->bind(1, std::int64_t{2});
                // Nothing bound to ?2: NULL, and the column is NOT NULL.
                EXPECT_FALSE(statement->run());
            }

            EXPECT_EQ(count_notes(database), 1);
        }

        TEST(SqliteDatabaseTest, ValuesRoundTripIncludingUnicodeAndNull) {
            SqliteDatabase database = in_memory();
            ASSERT_TRUE(database.execute("CREATE TABLE value (i INTEGER, d REAL, t TEXT, n TEXT)"));

            {
                Result<Statement> insert = database.prepare("INSERT INTO value VALUES (?1, ?2, ?3, ?4)");
                ASSERT_TRUE(insert);
                ASSERT_TRUE(insert->bind(1, std::int64_t{-42}).bind(2, 0.125).bind(3, "č 漢 👍").bind_null(4).run());
            }

            Result<Statement> select = database.prepare("SELECT i, d, t, n FROM value");
            ASSERT_TRUE(select);
            ASSERT_TRUE(select->step().value_or(false));
            EXPECT_EQ(select->column_int(0), -42);
            EXPECT_EQ(select->column_double(1), 0.125);
            EXPECT_EQ(select->column_text(2), "č 漢 👍");
            EXPECT_TRUE(select->column_is_null(3));
            EXPECT_EQ(select->column_count(), 4);
        }

        TEST(SqliteDatabaseTest, AStatementErrorCarriesSqlitesOwnMessage) {
            // Ours would say less. SQLite names the table.
            SqliteDatabase database = in_memory();

            const Result<Statement> statement = database.prepare("SELECT * FROM nonexistent");

            ASSERT_FALSE(statement);
            EXPECT_EQ(statement.error().code, ErrorCode::DbQuery);
            EXPECT_NE(statement.error().context.find("no such table"), std::string::npos) << statement.error().context;
            EXPECT_NE(statement.error().context.find("nonexistent"), std::string::npos) << statement.error().context;
        }

        TEST(SqliteDatabaseTest, RunRefusesAStatementThatReturnsRows) {
            SqliteDatabase database = in_memory();
            make_table(database);
            ASSERT_TRUE(insert_note(database, 1, "one"));

            Result<Statement> select = database.prepare("SELECT id FROM note");
            ASSERT_TRUE(select);

            const Status ran = select->run();

            ASSERT_FALSE(ran) << "a caller ignoring rows has misread their own query";
            EXPECT_NE(ran.error().context.find("returned rows"), std::string::npos) << ran.error().context;
        }

        // ---- transactions --------------------------------------------------

        TEST(SqliteDatabaseTest, ATransactionCommitsWhatItWasTold) {
            SqliteDatabase database = in_memory();
            make_table(database);

            {
                Result<Transaction> transaction = database.begin();
                ASSERT_TRUE(transaction);
                ASSERT_TRUE(insert_note(database, 1, "kept"));
                ASSERT_TRUE(transaction->commit());
            }

            EXPECT_EQ(count_notes(database), 1);
            EXPECT_FALSE(database.in_transaction());
        }

        TEST(SqliteDatabaseTest, ATransactionRollsBackWhenItIsNotCommitted) {
            SqliteDatabase database = in_memory();
            make_table(database);

            {
                Result<Transaction> transaction = database.begin();
                ASSERT_TRUE(transaction);
                ASSERT_TRUE(insert_note(database, 1, "discarded"));
            }

            EXPECT_EQ(count_notes(database), 0);
            EXPECT_FALSE(database.in_transaction());
        }

        /// Thrown to leave a scope, and nothing else. Deliberately not
        /// std::runtime_error: on Ubuntu's libc++, its message is allocated by
        /// operator new inside libc++ and freed with free() inside libc++abi,
        /// which ASan correctly reports as an alloc-dealloc mismatch that has
        /// nothing to do with this test. The test is about unwinding.
        struct MidSaveFailure {};

        TEST(SqliteDatabaseTest, ATransactionRollsBackWhileUnwinding) {
            // The path that matters: a write that throws halfway through must
            // leave the database exactly as it was.
            SqliteDatabase database = in_memory();
            make_table(database);

            try {
                Result<Transaction> transaction = database.begin();
                ASSERT_TRUE(transaction);
                ASSERT_TRUE(insert_note(database, 1, "half a session"));
                throw MidSaveFailure{};
            } catch (const MidSaveFailure&) {
                // Swallowed on purpose; the assertion is below.
            }

            EXPECT_EQ(count_notes(database), 0);
            EXPECT_FALSE(database.in_transaction());
        }

        TEST(SqliteDatabaseTest, ASecondTransactionIsRefusedRatherThanNested) {
            // Documented choice: every write path here is one transaction deep,
            // and a nesting scheme nobody needs is a way to commit half a
            // session by accident.
            SqliteDatabase database = in_memory();
            Result<Transaction> first = database.begin();
            ASSERT_TRUE(first);

            const Result<Transaction> second = database.begin();

            ASSERT_FALSE(second);
            EXPECT_NE(second.error().context.find("already open"), std::string::npos) << second.error().context;
        }

        TEST(SqliteDatabaseTest, ATransactionCanBeOpenedAgainAfterTheFirstOneEnds) {
            SqliteDatabase database = in_memory();
            make_table(database);

            {
                Result<Transaction> first = database.begin();
                ASSERT_TRUE(first);
                ASSERT_TRUE(first->commit());
            }
            Result<Transaction> second = database.begin();

            ASSERT_TRUE(second);
            EXPECT_TRUE(insert_note(database, 1, "after"));
            EXPECT_TRUE(second->commit());
        }

        TEST(SqliteDatabaseTest, CommittingTwiceIsRefused) {
            SqliteDatabase database = in_memory();
            Result<Transaction> transaction = database.begin();
            ASSERT_TRUE(transaction);
            ASSERT_TRUE(transaction->commit());

            EXPECT_FALSE(transaction->commit());
            EXPECT_FALSE(transaction->is_open());
        }

        // ---- files ---------------------------------------------------------

        TEST(SqliteDatabaseTest, DataSurvivesClosingAndReopening) {
            const testing::TempEnv env;
            const std::filesystem::path file = env.data() / "history.db";
            {
                Result<SqliteDatabase> database = SqliteDatabase::open(file, SqliteDatabase::OpenMode::CreateIfMissing);
                ASSERT_TRUE(database);
                make_table(*database);
                Result<Transaction> transaction = database->begin();
                ASSERT_TRUE(transaction);
                ASSERT_TRUE(insert_note(*database, 1, "persisted"));
                ASSERT_TRUE(transaction->commit());
            }

            Result<SqliteDatabase> reopened = SqliteDatabase::open(file, SqliteDatabase::OpenMode::OpenExisting);

            ASSERT_TRUE(reopened) << (reopened ? "" : reopened.error().context);
            EXPECT_EQ(count_notes(*reopened), 1);
        }

        TEST(SqliteDatabaseTest, ASecondConnectionCanReadWhileTheFirstIsOpen) {
            // WAL's promise, and the reason the busy timeout exists: two
            // TypeIts on one history is a normal thing to do.
            const testing::TempEnv env;
            const std::filesystem::path file = env.data() / "shared.db";
            Result<SqliteDatabase> writer = SqliteDatabase::open(file, SqliteDatabase::OpenMode::CreateIfMissing);
            ASSERT_TRUE(writer);
            make_table(*writer);
            {
                Result<Transaction> transaction = writer->begin();
                ASSERT_TRUE(transaction);
                ASSERT_TRUE(insert_note(*writer, 1, "written"));
                ASSERT_TRUE(transaction->commit());
            }

            Result<SqliteDatabase> reader = SqliteDatabase::open(file, SqliteDatabase::OpenMode::OpenExisting);

            ASSERT_TRUE(reader) << (reader ? "" : reader.error().context);
            EXPECT_EQ(count_notes(*reader), 1);
        }

    }  // namespace
}  // namespace typeit::infra
