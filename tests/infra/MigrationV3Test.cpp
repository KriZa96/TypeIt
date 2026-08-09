// The first migration after the schema shipped (TX-006).
//
// TEXT_SOURCES calls it "schema v2" because it was written when v1 was the only
// one; it is version 3, because TI-109 took 2 for the daily-totals index.
//
// The acceptance criterion is one sentence: somebody upgrading from beta keeps
// every imported text and every bookmark. Everything below is that sentence
// checked against a database built by the old schema and nothing else — a test
// that migrated a database this binary had just created would be testing the
// binary against itself.

#include <cstdint>
#include <gtest/gtest.h>
#include <memory>
#include <set>
#include <string>
#include <string_view>
#include <utility>

#include "typeit/core/util/Result.h"
#include "typeit/infra/db/Migrator.h"
#include "typeit/infra/db/Schema.h"
#include "typeit/infra/db/SqliteDatabase.h"

namespace typeit::infra {
    namespace {

        using core::ErrorCode;
        using core::Result;
        using core::Status;

        constexpr int kSections = 3;

        SqliteDatabase in_memory() {
            Result<SqliteDatabase> database = SqliteDatabase::open_in_memory();
            EXPECT_TRUE(database) << (database ? "" : database.error().context);
            return std::move(*database);
        }

        /// A database at exactly the schema `stop` describes, and no further.
        ///
        /// Built by replaying the released migration files rather than by a
        /// hand-written copy of the old schema: a copy would drift from what
        /// actually shipped, and the whole point is to migrate the thing that
        /// shipped.
        void migrate_up_to(SqliteDatabase& database, int stop) {
            for (const Migration& migration: migrations()) {
                if (migration.version > stop) {
                    return;
                }
                ASSERT_TRUE(database.execute_script(migration.sql)) << migration.name;
                ASSERT_TRUE(database.execute(migration.stamp)) << migration.name;
            }
        }

        /// A text and a bookmark in the old shape — no sections, no
        /// `section_idx`, no `mime`.
        void insert_old_library(SqliteDatabase& database) {
            ASSERT_TRUE(database.execute(
                    "INSERT INTO text_item (id, title, source, origin, content, content_sha256, grapheme_count,"
                    " word_count, difficulty, created_at)"
                    " VALUES (1, 'Moby-Dick', 'file', '/books/moby.txt', 'Call me Ishmael.', 'hash-one', 16, 3,"
                    " 4.5, 1700000000000)"));
            ASSERT_TRUE(database.execute(
                    "INSERT INTO text_item (id, title, source, content, content_sha256, grapheme_count, word_count,"
                    " created_at) VALUES (2, 'a paste', 'paste', 'hello there', 'hash-two', 11, 2, 1700000000001)"));
            ASSERT_TRUE(
                    database.execute("INSERT INTO text_bookmark (text_id, offset, updated_at) VALUES (1, 9, 1700)"));
            ASSERT_TRUE(database.execute("INSERT INTO text_tag (text_id, tag) VALUES (1, 'classics')"));
        }

        std::set<std::string> columns_of(SqliteDatabase& database, std::string_view pragma) {
            std::set<std::string> names;
            Result<Statement> statement = database.prepare(pragma);
            EXPECT_TRUE(statement) << (statement ? "" : statement.error().context);
            while (statement->step().value_or(false)) {
                names.insert(statement->column_text(1));
            }
            return names;
        }

        // ---- what a beta database keeps -------------------------------------------

        TEST(MigrationV3Test, EveryRowSurvivesTheUpgrade) {
            SqliteDatabase database = in_memory();
            migrate_up_to(database, kSections - 1);
            insert_old_library(database);

            const Result<MigrationOutcome> outcome = migrate_to_latest(database);

            ASSERT_TRUE(outcome) << (outcome ? "" : outcome.error().context);
            EXPECT_EQ(outcome->from, kSections - 1);
            EXPECT_EQ(outcome->to, kSections);
            EXPECT_EQ(database.query_int("SELECT COUNT(*) FROM text_item").value_or(-1), 2);
            EXPECT_EQ(database.query_int("SELECT COUNT(*) FROM text_bookmark").value_or(-1), 1);
            EXPECT_EQ(database.query_int("SELECT COUNT(*) FROM text_tag").value_or(-1), 1);
        }

        TEST(MigrationV3Test, ATextThatWasHereBeforeGetsTheOneSectionItAlwaysHad) {
            // Without this backfill, a library imported before today would have
            // texts with no sections at all, and every reader would need an "or
            // none, for the old ones" branch — which is the branch TX-005
            // exists to remove.
            SqliteDatabase database = in_memory();
            migrate_up_to(database, kSections - 1);
            insert_old_library(database);

            ASSERT_TRUE(migrate_to_latest(database));

            EXPECT_EQ(database.query_int("SELECT COUNT(*) FROM text_section").value_or(-1), 2) << "one per text";
            Result<Statement> statement =
                    database.prepare("SELECT idx, title, start_idx, end_idx FROM text_section WHERE text_id = 1");
            ASSERT_TRUE(statement);
            ASSERT_TRUE(statement->step().value_or(false));
            EXPECT_EQ(statement->column_int(0), 0);
            EXPECT_TRUE(statement->column_is_null(1)) << "nobody named it, so it has no name";
            EXPECT_EQ(statement->column_int(2), 0);
            EXPECT_EQ(statement->column_int(3), 16) << "the whole text, in graphemes";
        }

        TEST(MigrationV3Test, AnExistingBookmarkKeepsItsOffsetAndGainsSectionZero) {
            SqliteDatabase database = in_memory();
            migrate_up_to(database, kSections - 1);
            insert_old_library(database);

            ASSERT_TRUE(migrate_to_latest(database));

            Result<Statement> statement =
                    database.prepare("SELECT offset, updated_at, section_idx FROM text_bookmark WHERE text_id = 1");
            ASSERT_TRUE(statement);
            ASSERT_TRUE(statement->step().value_or(false));
            EXPECT_EQ(statement->column_int(0), 9) << "where they actually stopped";
            EXPECT_EQ(statement->column_int(1), 1700);
            EXPECT_EQ(statement->column_int(2), 0);
        }

        TEST(MigrationV3Test, TheNewTextColumnsArriveEmptyRatherThanInvented) {
            // A text imported before the pipeline recorded its type has no type
            // to record. Guessing one from the origin's extension would be
            // writing a fact nobody established.
            SqliteDatabase database = in_memory();
            migrate_up_to(database, kSections - 1);
            insert_old_library(database);

            ASSERT_TRUE(migrate_to_latest(database));

            const std::set<std::string> columns = columns_of(database, "PRAGMA table_info(text_item)");
            EXPECT_TRUE(columns.contains("author"));
            EXPECT_TRUE(columns.contains("mime"));
            EXPECT_TRUE(columns.contains("extractor"));
            Result<Statement> statement =
                    database.prepare("SELECT author, mime, extractor FROM text_item WHERE id = 1");
            ASSERT_TRUE(statement);
            ASSERT_TRUE(statement->step().value_or(false));
            EXPECT_TRUE(statement->column_is_null(0));
            EXPECT_TRUE(statement->column_is_null(1));
            EXPECT_TRUE(statement->column_is_null(2));
        }

        TEST(MigrationV3Test, AnEmptyLibraryMigratesToAnEmptySectionTable) {
            // The backfill is a SELECT over the texts, so no texts is no rows
            // rather than one row about nothing.
            SqliteDatabase database = in_memory();
            migrate_up_to(database, kSections - 1);

            ASSERT_TRUE(migrate_to_latest(database));

            EXPECT_EQ(database.query_int("SELECT COUNT(*) FROM text_section").value_or(-1), 0);
        }

        // ---- when it goes wrong ---------------------------------------------------

        TEST(MigrationV3Test, AFailingUpgradeLeavesTheDatabaseAtTheVersionItWas) {
            // A table already occupying the name this migration wants. The
            // script fails partway and the transaction takes the whole step
            // back — so the next start retries this migration rather than the
            // one after it, and the rows are still there to retry against.
            SqliteDatabase database = in_memory();
            migrate_up_to(database, kSections - 1);
            insert_old_library(database);
            ASSERT_TRUE(database.execute("CREATE TABLE text_section (surprise TEXT)"));

            const Result<MigrationOutcome> outcome = migrate_to_latest(database);

            ASSERT_FALSE(outcome);
            EXPECT_EQ(outcome.error().code, ErrorCode::DbMigrate);
            EXPECT_EQ(schema_version(database).value_or(-1), kSections - 1) << "not bumped";
            EXPECT_EQ(database.query_int("SELECT COUNT(*) FROM text_item").value_or(-1), 2) << "and nothing was lost";
            const std::set<std::string> bookmark = columns_of(database, "PRAGMA table_info(text_bookmark)");
            EXPECT_FALSE(bookmark.contains("section_idx")) << "the ALTER went back with the rest of the step";
            EXPECT_FALSE(database.in_transaction());
        }

        TEST(MigrationV3Test, MigratingTwiceAppliesItOnce) {
            // Otherwise the backfill would insert a second section zero for
            // every text on the second run, and the primary key would be the
            // only thing stopping it.
            SqliteDatabase database = in_memory();
            migrate_up_to(database, kSections - 1);
            insert_old_library(database);
            ASSERT_TRUE(migrate_to_latest(database));

            const Result<MigrationOutcome> again = migrate_to_latest(database);

            ASSERT_TRUE(again) << (again ? "" : again.error().context);
            EXPECT_EQ(again->applied, 0);
            EXPECT_EQ(database.query_int("SELECT COUNT(*) FROM text_section").value_or(-1), 2);
        }

        // ---- the table's own rules -------------------------------------------------

        TEST(MigrationV3Test, DeletingATextTakesItsSectionsWithIt) {
            SqliteDatabase database = in_memory();
            ASSERT_TRUE(migrate_to_latest(database));
            ASSERT_TRUE(database.execute(
                    "INSERT INTO text_item (id, title, source, content, content_sha256, grapheme_count, word_count,"
                    " created_at) VALUES (1, 'a book', 'file', 'hello', 'hash', 5, 1, 0)"));
            ASSERT_TRUE(database.execute(
                    "INSERT INTO text_section (text_id, idx, title, start_idx, end_idx) VALUES (1, 0, 'One', 0, 5)"));

            ASSERT_TRUE(database.execute("DELETE FROM text_item WHERE id = 1"));

            EXPECT_EQ(database.query_int("SELECT COUNT(*) FROM text_section").value_or(-1), 0) << "ON DELETE CASCADE";
        }

        TEST(MigrationV3Test, TwoSectionsCannotShareAnIndexWithinOneText) {
            SqliteDatabase database = in_memory();
            ASSERT_TRUE(migrate_to_latest(database));
            ASSERT_TRUE(database.execute(
                    "INSERT INTO text_item (id, title, source, content, content_sha256, grapheme_count, word_count,"
                    " created_at) VALUES (1, 'a book', 'file', 'hello', 'hash', 5, 1, 0)"));
            ASSERT_TRUE(database.execute(
                    "INSERT INTO text_section (text_id, idx, start_idx, end_idx) VALUES (1, 0, 0, 5)"));

            const Status duplicate =
                    database.execute("INSERT INTO text_section (text_id, idx, start_idx, end_idx) VALUES (1, 0, 5, 5)");

            ASSERT_FALSE(duplicate);
            EXPECT_NE(duplicate.error().context.find("UNIQUE"), std::string::npos) << duplicate.error().context;
        }

        TEST(MigrationV3Test, ASectionCannotBelongToATextThatIsNotThere) {
            SqliteDatabase database = in_memory();
            ASSERT_TRUE(migrate_to_latest(database));

            const Status orphan =
                    database.execute("INSERT INTO text_section (text_id, idx, start_idx, end_idx) VALUES (9, 0, 0, 1)");

            ASSERT_FALSE(orphan) << "PRAGMA foreign_keys is on, and this is why";
        }

        TEST(MigrationV3Test, TheMigrationIsAboveEverythingReleased) {
            // What the CI schema guard enforces on a pull request, asserted
            // here as well: a new file numbered at or below a released one is
            // skipped by every database already past it, which is a migration
            // that silently never runs.
            EXPECT_EQ(latest_schema_version(), kSections);
            EXPECT_EQ(static_cast<int>(migrations().size()), kSections);
        }

    }  // namespace
}  // namespace typeit::infra
