#include <algorithm>
#include <cstdint>
#include <filesystem>
#include <gtest/gtest.h>
#include <memory>
#include <set>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "typeit/core/util/Result.h"
#include "typeit/infra/db/Migrator.h"
#include "typeit/infra/db/Schema.h"
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

        std::set<std::string> names_of(SqliteDatabase& database, std::string_view sql) {
            std::set<std::string> names;
            Result<Statement> statement = database.prepare(sql);
            EXPECT_TRUE(statement) << (statement ? "" : statement.error().context);
            while (statement->step().value_or(false)) {
                names.insert(statement->column_text(0));
            }
            return names;
        }

        std::set<std::string> tables_of(SqliteDatabase& database) {
            return names_of(database, "SELECT name FROM sqlite_master WHERE type = 'table' ORDER BY name");
        }

        std::set<std::string> indexes_of(SqliteDatabase& database) {
            return names_of(database,
                            "SELECT name FROM sqlite_master WHERE type = 'index' AND name NOT LIKE 'sqlite_%'");
        }

        // ---- migrating -----------------------------------------------------

        TEST(MigratorTest, AnEmptyDatabaseMigratesToTheLatestVersion) {
            SqliteDatabase database = in_memory();
            ASSERT_EQ(schema_version(database).value_or(-1), 0) << "a new database is version zero";

            const Result<MigrationOutcome> outcome = migrate_to_latest(database);

            ASSERT_TRUE(outcome) << (outcome ? "" : outcome.error().context);
            EXPECT_EQ(outcome->from, 0);
            EXPECT_EQ(outcome->to, latest_schema_version());
            EXPECT_EQ(outcome->applied, static_cast<int>(migrations().size()));
            EXPECT_EQ(schema_version(database).value_or(-1), latest_schema_version());
        }

        TEST(MigratorTest, AnAlreadyCurrentDatabaseIsANoOp) {
            SqliteDatabase database = in_memory();
            ASSERT_TRUE(migrate_to_latest(database));

            const Result<MigrationOutcome> again = migrate_to_latest(database);

            ASSERT_TRUE(again) << (again ? "" : again.error().context);
            EXPECT_EQ(again->applied, 0) << "nothing to do is not a failure";
            EXPECT_EQ(again->from, latest_schema_version());
            EXPECT_EQ(again->to, latest_schema_version());
        }

        TEST(MigratorTest, MigratingIsIdempotentAcrossReopens) {
            const testing::TempEnv env;
            const std::filesystem::path file = env.data() / "history.db";

            for (int run = 0; run < 3; ++run) {
                Result<SqliteDatabase> database = SqliteDatabase::open(file, SqliteDatabase::OpenMode::CreateIfMissing);
                ASSERT_TRUE(database) << (database ? "" : database.error().context);
                const Result<MigrationOutcome> outcome = migrate_to_latest(*database);
                ASSERT_TRUE(outcome) << (outcome ? "" : outcome.error().context);
                EXPECT_EQ(outcome->applied, run == 0 ? static_cast<int>(migrations().size()) : 0) << "run " << run;
                EXPECT_EQ(schema_version(*database).value_or(-1), latest_schema_version());
            }
        }

        TEST(MigratorTest, AFutureVersionIsRefusedRatherThanUpgradedOrWiped) {
            // Somebody's entire typing history is in there. The only safe
            // answer to "I do not understand this file" is to say so.
            SqliteDatabase database = in_memory();
            ASSERT_TRUE(migrate_to_latest(database));
            ASSERT_TRUE(database.execute("PRAGMA user_version = 999"));
            ASSERT_TRUE(database.execute("CREATE TABLE from_the_future (id INTEGER PRIMARY KEY)"));

            const Result<MigrationOutcome> outcome = migrate_to_latest(database);

            ASSERT_FALSE(outcome);
            EXPECT_EQ(outcome.error().code, ErrorCode::DbMigrate);
            EXPECT_NE(outcome.error().context.find("newer version"), std::string::npos) << outcome.error().context;
            EXPECT_EQ(schema_version(database).value_or(-1), 999) << "unchanged";
            EXPECT_TRUE(tables_of(database).contains("from_the_future")) << "and nothing was dropped";
        }

        TEST(MigratorTest, AFailingMigrationLeavesTheVersionAndTheSchemaAlone) {
            // A table already occupying a name the schema wants: the script
            // fails partway, and the transaction takes the whole step back.
            SqliteDatabase database = in_memory();
            ASSERT_TRUE(database.execute("CREATE TABLE session (surprise TEXT)"));

            const Result<MigrationOutcome> outcome = migrate_to_latest(database);

            ASSERT_FALSE(outcome);
            EXPECT_EQ(outcome.error().code, ErrorCode::DbMigrate);
            EXPECT_EQ(schema_version(database).value_or(-1), 0) << "still unmigrated";
            EXPECT_FALSE(tables_of(database).contains("text_item")) << "and no half-built schema was left behind";
            EXPECT_FALSE(database.in_transaction());
        }

        TEST(MigratorTest, TheErrorNamesTheMigrationThatFailed) {
            SqliteDatabase database = in_memory();
            ASSERT_TRUE(database.execute("CREATE TABLE session (surprise TEXT)"));

            const Result<MigrationOutcome> outcome = migrate_to_latest(database);

            ASSERT_FALSE(outcome);
            EXPECT_NE(outcome.error().context.find("001_initial.sql"), std::string::npos) << outcome.error().context;
        }

        TEST(MigratorTest, TheMigrationsAreOrderedAndUniquelyNumbered) {
            int previous = 0;
            for (const Migration& migration: migrations()) {
                EXPECT_GT(migration.version, previous) << migration.name;
                EXPECT_FALSE(migration.sql.empty()) << migration.name;
                previous = migration.version;
            }
            EXPECT_EQ(latest_schema_version(), previous);
        }

        // ---- schema v1 -----------------------------------------------------

        class SchemaTest : public ::testing::Test {
        protected:
            void SetUp() override {
                database_ = std::make_unique<SqliteDatabase>(in_memory());
                ASSERT_TRUE(migrate_to_latest(*database_));
            }

            SqliteDatabase& db() { return *database_; }

            std::unique_ptr<SqliteDatabase> database_;
        };

        TEST_F(SchemaTest, EveryTableInTechnicalSectionFiveExists) {
            // Introspected rather than assumed: the schema is the contract
            // between this binary and everybody's history file.
            const std::set<std::string> expected{
                    "profile",  "text_item",   "text_tag",   "text_bookmark", "session",        "session_sample",
                    "key_stat", "bigram_stat", "error_pair", "personal_best", "keystroke_blob",
            };

            EXPECT_EQ(tables_of(db()), expected);
        }

        TEST_F(SchemaTest, TheDocumentedIndexesExist) {
            const std::set<std::string> indexes = indexes_of(db());

            EXPECT_TRUE(indexes.contains("idx_session_started")) << "the history screen orders by this";
            EXPECT_TRUE(indexes.contains("idx_session_mode"));
        }

        TEST_F(SchemaTest, DeletingASessionTakesItsSamplesWithIt) {
            ASSERT_TRUE(db().execute(
                    "INSERT INTO session (id, started_at, ended_at, mode, provider, provider_seed, duration_ms,"
                    " graphemes_typed, graphemes_correct, errors_total, errors_uncorrected, backspaces, raw_wpm,"
                    " gross_wpm, net_wpm, accuracy, final_correctness, consistency, completed, app_version)"
                    " VALUES (1, 0, 1000, 'timed', 'whole', 7, 1000, 10, 10, 0, 0, 0, 60, 60, 60, 1, 1, 100, 1, '2')"));
            ASSERT_TRUE(
                    db().execute("INSERT INTO session_sample (session_id, t_ms, wpm, errors) VALUES (1, 0, 60, 0)"));
            ASSERT_EQ(db().query_int("SELECT COUNT(*) FROM session_sample").value_or(-1), 1);

            ASSERT_TRUE(db().execute("DELETE FROM session WHERE id = 1"));

            EXPECT_EQ(db().query_int("SELECT COUNT(*) FROM session_sample").value_or(-1), 0) << "ON DELETE CASCADE";
        }

        TEST_F(SchemaTest, DeletingATextLeavesItsSessionsWithoutOne) {
            // A text is removed from the library; the runs typed against it are
            // still runs, and their metrics are still yours.
            ASSERT_TRUE(db().execute(
                    "INSERT INTO text_item (id, title, source, content, content_sha256, grapheme_count, word_count,"
                    " created_at) VALUES (1, 'a text', 'file', 'hello', 'hash', 5, 1, 0)"));
            ASSERT_TRUE(db().execute(
                    "INSERT INTO session (id, started_at, ended_at, mode, text_id, provider, provider_seed,"
                    " duration_ms, graphemes_typed, graphemes_correct, errors_total, errors_uncorrected, backspaces,"
                    " raw_wpm, gross_wpm, net_wpm, accuracy, final_correctness, consistency, completed, app_version)"
                    " VALUES (1, 0, 1, 'quote', 1, 'whole', 7, 1, 1, 1, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, '2')"));

            ASSERT_TRUE(db().execute("DELETE FROM text_item WHERE id = 1"));

            EXPECT_EQ(db().query_int("SELECT COUNT(*) FROM session").value_or(-1), 1) << "the run survives";
            Result<Statement> statement = db().prepare("SELECT text_id FROM session WHERE id = 1");
            ASSERT_TRUE(statement);
            ASSERT_TRUE(statement->step().value_or(false));
            EXPECT_TRUE(statement->column_is_null(0)) << "ON DELETE SET NULL";
        }

        TEST_F(SchemaTest, DeletingATextTakesItsTagsAndBookmark) {
            ASSERT_TRUE(db().execute(
                    "INSERT INTO text_item (id, title, source, content, content_sha256, grapheme_count, word_count,"
                    " created_at) VALUES (1, 'a text', 'paste', 'hello', 'hash', 5, 1, 0)"));
            ASSERT_TRUE(db().execute("INSERT INTO text_tag (text_id, tag) VALUES (1, 'prose')"));
            ASSERT_TRUE(db().execute("INSERT INTO text_bookmark (text_id, offset, updated_at) VALUES (1, 3, 0)"));

            ASSERT_TRUE(db().execute("DELETE FROM text_item WHERE id = 1"));

            EXPECT_EQ(db().query_int("SELECT COUNT(*) FROM text_tag").value_or(-1), 0);
            EXPECT_EQ(db().query_int("SELECT COUNT(*) FROM text_bookmark").value_or(-1), 0);
        }

        TEST_F(SchemaTest, TheSourceCheckRejectsAnUnknownOrigin) {
            const Status inserted = db().execute(
                    "INSERT INTO text_item (id, title, source, content, content_sha256, grapheme_count, word_count,"
                    " created_at) VALUES (1, 'a text', 'telepathy', 'hello', 'hash', 5, 1, 0)");

            ASSERT_FALSE(inserted);
            EXPECT_NE(inserted.error().context.find("CHECK"), std::string::npos) << inserted.error().context;
        }

        TEST_F(SchemaTest, TheCompletedCheckRejectsAnythingButZeroOrOne) {
            const Status inserted = db().execute(
                    "INSERT INTO session (id, started_at, ended_at, mode, provider, provider_seed, duration_ms,"
                    " graphemes_typed, graphemes_correct, errors_total, errors_uncorrected, backspaces, raw_wpm,"
                    " gross_wpm, net_wpm, accuracy, final_correctness, consistency, completed, app_version)"
                    " VALUES (1, 0, 1, 'timed', 'whole', 7, 1, 1, 1, 0, 0, 0, 1, 1, 1, 1, 1, 1, 2, '2')");

            ASSERT_FALSE(inserted);
            EXPECT_NE(inserted.error().context.find("CHECK"), std::string::npos) << inserted.error().context;
        }

        TEST_F(SchemaTest, TheProfileCheckAllowsExactlyOneRow) {
            ASSERT_TRUE(db().execute("INSERT INTO profile (id, created_at) VALUES (1, 0)"));

            EXPECT_FALSE(db().execute("INSERT INTO profile (id, created_at) VALUES (2, 0)"));
        }

        TEST_F(SchemaTest, TheContentHashIsUnique) {
            // What makes importing the same file twice one text rather than two.
            ASSERT_TRUE(db().execute(
                    "INSERT INTO text_item (id, title, source, content, content_sha256, grapheme_count, word_count,"
                    " created_at) VALUES (1, 'a text', 'file', 'hello', 'same-hash', 5, 1, 0)"));

            const Status duplicate = db().execute(
                    "INSERT INTO text_item (id, title, source, content, content_sha256, grapheme_count, word_count,"
                    " created_at) VALUES (2, 'same text again', 'file', 'hello', 'same-hash', 5, 1, 0)");

            ASSERT_FALSE(duplicate);
            EXPECT_NE(duplicate.error().context.find("UNIQUE"), std::string::npos) << duplicate.error().context;
        }

        TEST_F(SchemaTest, UnicodeSurvivesTheRoundTrip) {
            ASSERT_TRUE(db().execute("INSERT INTO key_stat VALUES ('č', 10, 1, 500)"));
            ASSERT_TRUE(db().execute("INSERT INTO key_stat VALUES ('👍', 3, 0, 90)"));

            Result<Statement> statement = db().prepare("SELECT grapheme FROM key_stat ORDER BY attempts DESC");
            ASSERT_TRUE(statement);
            ASSERT_TRUE(statement->step().value_or(false));
            EXPECT_EQ(statement->column_text(0), "č");
            ASSERT_TRUE(statement->step().value_or(false));
            EXPECT_EQ(statement->column_text(0), "👍");
        }

    }  // namespace
}  // namespace typeit::infra
