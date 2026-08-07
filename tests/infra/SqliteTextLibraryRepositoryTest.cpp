#include <cstdint>
#include <gtest/gtest.h>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "typeit/app/records/TextLibrary.h"
#include "typeit/core/util/Result.h"
#include "typeit/core/util/Units.h"
#include "typeit/infra/db/Migrator.h"
#include "typeit/infra/db/SqliteDatabase.h"
#include "typeit/infra/db/SqliteTextLibraryRepository.h"

namespace typeit::infra {
    namespace {

        using core::Result;
        using core::Status;

        constexpr core::Millis kNoon{1'767'225'600'000};

        app::TextItem a_text(std::string title, std::string hash) {
            app::TextItem text;
            text.title = std::move(title);
            text.source = app::TextSource::File;
            text.origin = "/home/kim/prose.txt";
            text.content = "the quick brown fox";
            text.content_sha256 = std::move(hash);
            text.language = "en";
            text.grapheme_count = 19;
            text.word_count = 4;
            text.difficulty = 3.5;
            text.created_at = kNoon;
            return text;
        }

        app::TextFilter tagged(std::vector<std::string> tags) {
            app::TextFilter filter;
            filter.tags = std::move(tags);
            return filter;
        }

        app::TextFilter searching(std::string term) {
            app::TextFilter filter;
            filter.search = std::move(term);
            return filter;
        }

        class LibraryTest : public ::testing::Test {
        protected:
            void SetUp() override {
                Result<SqliteDatabase> database = SqliteDatabase::open_in_memory();
                ASSERT_TRUE(database) << (database ? "" : database.error().context);
                database_ = std::make_unique<SqliteDatabase>(std::move(*database));
                ASSERT_TRUE(migrate_to_latest(*database_));
                repository_ = std::make_unique<SqliteTextLibraryRepository>(*database_);
            }

            [[nodiscard]] std::int64_t count(std::string_view sql) {
                const Result<std::int64_t> value = database_->query_int(sql);
                EXPECT_TRUE(value) << (value ? "" : value.error().context);
                return value.value_or(-1);
            }

            core::TextId add(const app::TextItem& text) {
                const Result<core::TextId> id = repository_->add(text);
                EXPECT_TRUE(id) << (id ? "" : id.error().context);
                return id.value_or(core::TextId{0});
            }

            std::unique_ptr<SqliteDatabase> database_;
            std::unique_ptr<SqliteTextLibraryRepository> repository_;
        };

        // ---- adding and reading --------------------------------------------

        TEST_F(LibraryTest, AddThenGetRoundTripsEveryField) {
            const app::TextItem original = a_text("Prose", "hash-1");

            const core::TextId id = add(original);
            const Result<std::optional<app::TextItem>> read = repository_->get(id);

            ASSERT_TRUE(read) << (read ? "" : read.error().context);
            ASSERT_TRUE(read->has_value());
            const app::TextItem& text = **read;
            EXPECT_EQ(text.id, id);
            EXPECT_EQ(text.title, "Prose");
            EXPECT_EQ(text.source, app::TextSource::File);
            EXPECT_EQ(text.origin, "/home/kim/prose.txt");
            EXPECT_EQ(text.content, "the quick brown fox");
            EXPECT_EQ(text.content_sha256, "hash-1");
            EXPECT_EQ(text.language, "en");
            EXPECT_EQ(text.grapheme_count, 19U);
            EXPECT_EQ(text.word_count, 4U);
            ASSERT_TRUE(text.difficulty.has_value());
            EXPECT_DOUBLE_EQ(*text.difficulty, 3.5);
            EXPECT_EQ(text.created_at, kNoon);
        }

        TEST_F(LibraryTest, TheOptionalFieldsRoundTripAsAbsent) {
            app::TextItem sparse = a_text("Pasted", "hash-sparse");
            sparse.source = app::TextSource::Paste;
            sparse.origin.reset();
            sparse.language.reset();
            sparse.difficulty.reset();

            const core::TextId id = add(sparse);
            const Result<std::optional<app::TextItem>> read = repository_->get(id);

            ASSERT_TRUE(read);
            ASSERT_TRUE(read->has_value());
            EXPECT_FALSE((*read)->origin.has_value()) << "absent, not an empty string";
            EXPECT_FALSE((*read)->language.has_value());
            EXPECT_FALSE((*read)->difficulty.has_value());
            EXPECT_EQ((*read)->source, app::TextSource::Paste);
        }

        TEST_F(LibraryTest, AMegabyteOfNonAsciiSurvives) {
            // A real import: a novel is this size, and it is not ASCII.
            std::string content;
            while (content.size() < 1'000'000) {
                content += "čšž 漢字 😀 the quick brown fox jumps over the lazy dog. ";
            }
            app::TextItem big = a_text("A whole book", "hash-big");
            big.content = content;
            big.content_raw = content;

            const core::TextId id = add(big);
            const Result<std::optional<app::TextItem>> read = repository_->get(id);

            ASSERT_TRUE(read) << (read ? "" : read.error().context);
            ASSERT_TRUE(read->has_value());
            EXPECT_EQ((*read)->content.size(), content.size());
            EXPECT_EQ((*read)->content, content) << "byte for byte";
        }

        TEST_F(LibraryTest, GettingATextThatIsNotThereIsAbsentRatherThanAnError) {
            // Asking about a text somebody else removed is a normal race.
            const Result<std::optional<app::TextItem>> read = repository_->get(core::TextId{999});

            ASSERT_TRUE(read) << (read ? "" : read.error().context);
            EXPECT_FALSE(read->has_value());
        }

        // ---- deduplication -------------------------------------------------

        TEST_F(LibraryTest, TheSameContentTwiceIsRefusedByTheHash) {
            add(a_text("Prose", "same-hash"));

            const Result<core::TextId> duplicate = repository_->add(a_text("Prose again", "same-hash"));

            ASSERT_FALSE(duplicate);
            EXPECT_NE(duplicate.error().context.find("UNIQUE"), std::string::npos) << duplicate.error().context;
            EXPECT_EQ(count("SELECT COUNT(*) FROM text_item"), 1);
        }

        TEST_F(LibraryTest, FindByHashReturnsTheTextAlreadyThere) {
            // What an importer calls before adding: importing the same file
            // twice should open the text you already have.
            const core::TextId id = add(a_text("Prose", "hash-1"));

            const Result<std::optional<app::TextItem>> found = repository_->find_by_hash("hash-1");

            ASSERT_TRUE(found) << (found ? "" : found.error().context);
            ASSERT_TRUE(found->has_value());
            EXPECT_EQ((*found)->id, id);
        }

        TEST_F(LibraryTest, FindByHashOfSomethingNeverImportedIsAbsent) {
            const Result<std::optional<app::TextItem>> found = repository_->find_by_hash("never-seen");

            ASSERT_TRUE(found);
            EXPECT_FALSE(found->has_value());
        }

        // ---- listing -------------------------------------------------------

        TEST_F(LibraryTest, ListLeavesTheContentBehind) {
            add(a_text("Prose", "hash-1"));

            const Result<std::vector<app::TextSummary>> summaries = repository_->list({});

            ASSERT_TRUE(summaries) << (summaries ? "" : summaries.error().context);
            ASSERT_EQ(summaries->size(), 1U);
            EXPECT_EQ(summaries->front().title, "Prose");
            EXPECT_EQ(summaries->front().grapheme_count, 19U);
        }

        TEST_F(LibraryTest, ListOrdersNewestFirst) {
            for (const std::int64_t offset: {0, 1'000, 2'000}) {
                app::TextItem text = a_text("Text " + std::to_string(offset), "hash-" + std::to_string(offset));
                text.created_at = kNoon + core::Millis{offset};
                add(text);
            }

            const Result<std::vector<app::TextSummary>> summaries = repository_->list({});

            ASSERT_TRUE(summaries);
            ASSERT_EQ(summaries->size(), 3U);
            EXPECT_EQ(summaries->at(0).created_at, kNoon + core::Millis{2'000});
            EXPECT_EQ(summaries->at(2).created_at, kNoon);
        }

        TEST_F(LibraryTest, ListHonoursTheLimit) {
            for (int i = 0; i < 5; ++i) {
                add(a_text("Text " + std::to_string(i), "hash-" + std::to_string(i)));
            }

            app::TextFilter filter;
            filter.limit = 2;

            EXPECT_EQ(repository_->list(filter).value_or(std::vector<app::TextSummary>{}).size(), 2U);
        }

        TEST_F(LibraryTest, ListSearchesTheTitle) {
            add(a_text("The Rust Book", "hash-rust"));
            add(a_text("Moby Dick", "hash-moby"));

            const Result<std::vector<app::TextSummary>> summaries = repository_->list(searching("rust"));

            ASSERT_TRUE(summaries) << (summaries ? "" : summaries.error().context);
            ASSERT_EQ(summaries->size(), 1U);
            EXPECT_EQ(summaries->front().title, "The Rust Book") << "and case-insensitively, for ASCII";
        }

        TEST_F(LibraryTest, ListSearchesTheTagsAsWellAsTheTitle) {
            // Somebody who tagged a text "rust" and called it something else
            // will type "rust" and expect to find it.
            const core::TextId tagged_text = add(a_text("Chapter One", "hash-one"));
            add(a_text("Chapter Two", "hash-two"));
            ASSERT_TRUE(repository_->tag(tagged_text, "rust"));

            const Result<std::vector<app::TextSummary>> summaries = repository_->list(searching("rust"));

            ASSERT_TRUE(summaries) << (summaries ? "" : summaries.error().context);
            ASSERT_EQ(summaries->size(), 1U);
            EXPECT_EQ(summaries->front().id, tagged_text);
        }

        TEST_F(LibraryTest, ASearchAndATagFilterNarrowTogether) {
            // The search finds and the filter narrows: an OR across title and
            // tags, ANDed with the tags asked for.
            const core::TextId wanted = add(a_text("Rust Book", "hash-a"));
            const core::TextId other = add(a_text("Rust Notes", "hash-b"));
            ASSERT_TRUE(repository_->tag(wanted, "long"));
            ASSERT_TRUE(repository_->tag(other, "short"));

            app::TextFilter filter;
            filter.search = "rust";
            filter.tags = {"long"};
            const Result<std::vector<app::TextSummary>> summaries = repository_->list(filter);

            ASSERT_TRUE(summaries);
            ASSERT_EQ(summaries->size(), 1U);
            EXPECT_EQ(summaries->front().id, wanted);
        }

        TEST_F(LibraryTest, ANonAsciiTitleIsFoundByAnExactSearch) {
            // `LIKE` is SQLite's, so case folding is ASCII-only: "Č" will not
            // match "č" and fixing that needs ICU. An exact match does work,
            // which is most of what anybody types — asserted so the limitation
            // is a known shape rather than a surprise.
            add(a_text("Čitanka", "hash-cir"));

            const Result<std::vector<app::TextSummary>> exact = repository_->list(searching("Čitanka"));

            ASSERT_TRUE(exact);
            ASSERT_EQ(exact->size(), 1U);
            EXPECT_EQ(exact->front().title, "Čitanka");
        }

        TEST_F(LibraryTest, ASearchThatMatchesNothingIsEmptyRatherThanAnError) {
            add(a_text("Moby Dick", "hash-moby"));

            const Result<std::vector<app::TextSummary>> summaries = repository_->list(searching("nothing here"));

            ASSERT_TRUE(summaries);
            EXPECT_TRUE(summaries->empty());
        }

        TEST_F(LibraryTest, ListFiltersByEveryTagAskedFor) {
            const core::TextId both = add(a_text("Both", "hash-both"));
            const core::TextId one = add(a_text("One", "hash-one"));
            ASSERT_TRUE(repository_->tag(both, "prose"));
            ASSERT_TRUE(repository_->tag(both, "long"));
            ASSERT_TRUE(repository_->tag(one, "prose"));

            const Result<std::vector<app::TextSummary>> summaries = repository_->list(tagged({"prose", "long"}));

            ASSERT_TRUE(summaries) << (summaries ? "" : summaries.error().context);
            ASSERT_EQ(summaries->size(), 1U) << "every tag, not any tag";
            EXPECT_EQ(summaries->front().id, both);
        }

        TEST_F(LibraryTest, AListingCarriesEachTextsTags) {
            const core::TextId id = add(a_text("Prose", "hash-1"));
            ASSERT_TRUE(repository_->tag(id, "prose"));
            ASSERT_TRUE(repository_->tag(id, "favourite"));

            const Result<std::vector<app::TextSummary>> summaries = repository_->list({});

            ASSERT_TRUE(summaries);
            ASSERT_EQ(summaries->size(), 1U);
            EXPECT_EQ(summaries->front().tags, (std::vector<std::string>{"favourite", "prose"}));
        }

        TEST_F(LibraryTest, ATagWithAQuoteInItStillFilters) {
            // The tag list is passed as JSON, so a tag containing a quote or a
            // backslash has to survive being written into it.
            const core::TextId id = add(a_text("Prose", "hash-1"));
            ASSERT_TRUE(repository_->tag(id, R"(say "what")"));

            const Result<std::vector<app::TextSummary>> summaries = repository_->list(tagged({R"(say "what")"}));

            ASSERT_TRUE(summaries) << (summaries ? "" : summaries.error().context);
            EXPECT_EQ(summaries->size(), 1U);
        }

        TEST_F(LibraryTest, AnEmptyLibraryListsNothing) {
            const Result<std::vector<app::TextSummary>> summaries = repository_->list({});

            ASSERT_TRUE(summaries) << (summaries ? "" : summaries.error().context);
            EXPECT_TRUE(summaries->empty());
        }

        // ---- tags ----------------------------------------------------------

        TEST_F(LibraryTest, TaggingTwiceIsSomebodyClickingTwice) {
            const core::TextId id = add(a_text("Prose", "hash-1"));

            ASSERT_TRUE(repository_->tag(id, "prose"));
            EXPECT_TRUE(repository_->tag(id, "prose")) << "not an error";

            EXPECT_EQ(count("SELECT COUNT(*) FROM text_tag"), 1);
        }

        TEST_F(LibraryTest, UntaggingRemovesOnlyThatTag) {
            const core::TextId id = add(a_text("Prose", "hash-1"));
            ASSERT_TRUE(repository_->tag(id, "prose"));
            ASSERT_TRUE(repository_->tag(id, "long"));

            ASSERT_TRUE(repository_->untag(id, "prose"));

            EXPECT_EQ(count("SELECT COUNT(*) FROM text_tag"), 1);
            EXPECT_TRUE(repository_->untag(id, "never-applied")) << "removing what is not there is not an error";
        }

        // ---- bookmarks -----------------------------------------------------

        TEST_F(LibraryTest, ABookmarkIsUpdatedRatherThanDuplicated) {
            const core::TextId id = add(a_text("A whole book", "hash-1"));

            ASSERT_TRUE(repository_->set_bookmark(
                    {.text_id = id, .offset = core::GraphemeIndex{100}, .updated_at = kNoon}));
            ASSERT_TRUE(repository_->set_bookmark(
                    {.text_id = id, .offset = core::GraphemeIndex{250}, .updated_at = kNoon + core::Millis{60'000}}));

            EXPECT_EQ(count("SELECT COUNT(*) FROM text_bookmark"), 1) << "one bookmark per text";
            const Result<std::optional<app::Bookmark>> mark = repository_->bookmark(id);
            ASSERT_TRUE(mark);
            ASSERT_TRUE(mark->has_value());
            EXPECT_EQ((*mark)->offset, core::GraphemeIndex{250});
            EXPECT_EQ((*mark)->updated_at, kNoon + core::Millis{60'000});
        }

        TEST_F(LibraryTest, ATextWithNoBookmarkHasNone) {
            const core::TextId id = add(a_text("Prose", "hash-1"));

            const Result<std::optional<app::Bookmark>> mark = repository_->bookmark(id);

            ASSERT_TRUE(mark) << (mark ? "" : mark.error().context);
            EXPECT_FALSE(mark->has_value());
        }

        // ---- removal -------------------------------------------------------

        TEST_F(LibraryTest, RemovingATextTakesItsTagsAndBookmark) {
            const core::TextId id = add(a_text("Prose", "hash-1"));
            ASSERT_TRUE(repository_->tag(id, "prose"));
            ASSERT_TRUE(
                    repository_->set_bookmark({.text_id = id, .offset = core::GraphemeIndex{10}, .updated_at = kNoon}));

            ASSERT_TRUE(repository_->remove(id));

            EXPECT_EQ(count("SELECT COUNT(*) FROM text_item"), 0);
            EXPECT_EQ(count("SELECT COUNT(*) FROM text_tag"), 0);
            EXPECT_EQ(count("SELECT COUNT(*) FROM text_bookmark"), 0);
        }

        TEST_F(LibraryTest, RemovingATextLeavesTheRunsTypedAgainstIt) {
            // Removing a text from the library is not disowning the runs: the
            // metrics are still yours, the text is just gone.
            const core::TextId id = add(a_text("Prose", "hash-1"));
            ASSERT_TRUE(database_->execute(
                    "INSERT INTO session (id, started_at, ended_at, mode, text_id, provider, provider_seed,"
                    " duration_ms, graphemes_typed, graphemes_correct, errors_total, errors_uncorrected, backspaces,"
                    " raw_wpm, gross_wpm, net_wpm, accuracy, final_correctness, consistency, completed, app_version)"
                    " VALUES (1, 0, 1, 'quote', 1, 'whole', 7, 1, 1, 1, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 'x')"));

            ASSERT_TRUE(repository_->remove(id));

            EXPECT_EQ(count("SELECT COUNT(*) FROM session"), 1) << "the run survives";
            EXPECT_EQ(count("SELECT COUNT(*) FROM session WHERE text_id IS NULL"), 1);
        }

        TEST_F(LibraryTest, RemovingSomethingThatIsNotThereIsNotAnError) {
            EXPECT_TRUE(repository_->remove(core::TextId{999}));
        }

    }  // namespace
}  // namespace typeit::infra
