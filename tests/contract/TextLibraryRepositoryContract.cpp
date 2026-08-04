// One suite, every implementation of ITextLibraryRepository.

#include <gtest/gtest.h>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "typeit/app/ports/ITextLibraryRepository.h"
#include "typeit/app/records/TextLibrary.h"
#include "typeit/core/util/Result.h"
#include "typeit/core/util/Units.h"
#include "typeit/infra/db/Migrator.h"
#include "typeit/infra/db/SqliteDatabase.h"
#include "typeit/infra/db/SqliteTextLibraryRepository.h"
#include "typeit/testing/Fakes.h"

namespace typeit {
    namespace {

        constexpr core::Millis kNoon{1'767'225'600'000};

        class SqliteLibraryFactory {
        public:
            SqliteLibraryFactory() {
                core::Result<infra::SqliteDatabase> database = infra::SqliteDatabase::open_in_memory();
                EXPECT_TRUE(database) << (database ? "" : database.error().context);
                database_ = std::make_unique<infra::SqliteDatabase>(std::move(*database));
                EXPECT_TRUE(infra::migrate_to_latest(*database_));
                repository_ = std::make_unique<infra::SqliteTextLibraryRepository>(*database_);
            }

            app::ITextLibraryRepository& repository() { return *repository_; }

        private:
            std::unique_ptr<infra::SqliteDatabase> database_;
            std::unique_ptr<infra::SqliteTextLibraryRepository> repository_;
        };

        class FakeLibraryFactory {
        public:
            app::ITextLibraryRepository& repository() { return repository_; }

        private:
            testing::FakeTextLibraryRepository repository_;
        };

        template<typename FactoryType>
        class TextLibraryRepositoryContract : public ::testing::Test {
        protected:
            app::ITextLibraryRepository& repository() { return factory_.repository(); }

            [[nodiscard]] static app::TextItem a_text(std::string title, std::string hash) {
                app::TextItem text;
                text.title = std::move(title);
                text.source = app::TextSource::File;
                text.origin = "/home/kim/prose.txt";
                text.content = "the quick brown fox";
                text.content_sha256 = std::move(hash);
                text.grapheme_count = 19;
                text.word_count = 4;
                text.created_at = kNoon;
                return text;
            }

            core::TextId add(const app::TextItem& text) {
                const core::Result<core::TextId> id = this->repository().add(text);
                EXPECT_TRUE(id) << (id ? "" : id.error().context);
                return id.value_or(core::TextId{0});
            }

            FactoryType factory_;
        };

        using LibraryImplementations = ::testing::Types<SqliteLibraryFactory, FakeLibraryFactory>;

        class LibraryNames {
        public:
            template<typename Factory>
            static std::string GetName(int /*index*/) {
                return std::is_same_v<Factory, SqliteLibraryFactory> ? "Sqlite" : "Fake";
            }
        };

        TYPED_TEST_SUITE(TextLibraryRepositoryContract, LibraryImplementations, LibraryNames);

        TYPED_TEST(TextLibraryRepositoryContract, AnEmptyLibraryAnswersEmpty) {
            const core::Result<std::vector<app::TextSummary>> summaries = this->repository().list({});
            const core::Result<std::optional<app::TextItem>> missing = this->repository().get(core::TextId{1});

            ASSERT_TRUE(summaries) << (summaries ? "" : summaries.error().context);
            ASSERT_TRUE(missing) << (missing ? "" : missing.error().context);
            EXPECT_TRUE(summaries->empty());
            EXPECT_FALSE(missing->has_value()) << "absent, not an error";
        }

        TYPED_TEST(TextLibraryRepositoryContract, AddThenGetRoundTrips) {
            const core::TextId id = this->add(TestFixture::a_text("Prose", "hash-1"));

            const core::Result<std::optional<app::TextItem>> read = this->repository().get(id);

            ASSERT_TRUE(read) << (read ? "" : read.error().context);
            ASSERT_TRUE(read->has_value());
            EXPECT_EQ((*read)->id, id);
            EXPECT_EQ((*read)->title, "Prose");
            EXPECT_EQ((*read)->content, "the quick brown fox");
            EXPECT_EQ((*read)->source, app::TextSource::File);
        }

        TYPED_TEST(TextLibraryRepositoryContract, TheSameContentTwiceIsRefused) {
            this->add(TestFixture::a_text("Prose", "same"));

            const core::Result<core::TextId> duplicate = this->repository().add(TestFixture::a_text("Again", "same"));

            EXPECT_FALSE(duplicate) << "the hash is what makes importing a file twice one text";
        }

        TYPED_TEST(TextLibraryRepositoryContract, FindByHashFindsWhatIsThereAndNothingElse) {
            const core::TextId id = this->add(TestFixture::a_text("Prose", "hash-1"));

            const core::Result<std::optional<app::TextItem>> found = this->repository().find_by_hash("hash-1");
            const core::Result<std::optional<app::TextItem>> absent = this->repository().find_by_hash("never");

            ASSERT_TRUE(found);
            ASSERT_TRUE(absent);
            ASSERT_TRUE(found->has_value());
            EXPECT_EQ((*found)->id, id);
            EXPECT_FALSE(absent->has_value());
        }

        TYPED_TEST(TextLibraryRepositoryContract, ListingOrdersNewestFirstAndHonoursTheLimit) {
            for (const std::int64_t offset: {0, 1'000, 2'000}) {
                app::TextItem text =
                        TestFixture::a_text("Text " + std::to_string(offset), "hash-" + std::to_string(offset));
                text.created_at = kNoon + core::Millis{offset};
                this->add(text);
            }

            app::TextFilter filter;
            filter.limit = 2;
            const core::Result<std::vector<app::TextSummary>> summaries = this->repository().list(filter);

            ASSERT_TRUE(summaries) << (summaries ? "" : summaries.error().context);
            ASSERT_EQ(summaries->size(), 2U);
            EXPECT_EQ(summaries->at(0).created_at, kNoon + core::Millis{2'000});
            EXPECT_EQ(summaries->at(1).created_at, kNoon + core::Millis{1'000});
        }

        TYPED_TEST(TextLibraryRepositoryContract, ListingSearchesTheTitleCaseInsensitively) {
            this->add(TestFixture::a_text("The Rust Book", "hash-rust"));
            this->add(TestFixture::a_text("Moby Dick", "hash-moby"));

            app::TextFilter filter;
            filter.search = "rust";
            const core::Result<std::vector<app::TextSummary>> summaries = this->repository().list(filter);

            ASSERT_TRUE(summaries) << (summaries ? "" : summaries.error().context);
            ASSERT_EQ(summaries->size(), 1U);
            EXPECT_EQ(summaries->front().title, "The Rust Book");
        }

        TYPED_TEST(TextLibraryRepositoryContract, ListingByTagWantsEveryTag) {
            const core::TextId both = this->add(TestFixture::a_text("Both", "hash-both"));
            const core::TextId one = this->add(TestFixture::a_text("One", "hash-one"));
            ASSERT_TRUE(this->repository().tag(both, "prose"));
            ASSERT_TRUE(this->repository().tag(both, "long"));
            ASSERT_TRUE(this->repository().tag(one, "prose"));

            app::TextFilter filter;
            filter.tags = {"prose", "long"};
            const core::Result<std::vector<app::TextSummary>> summaries = this->repository().list(filter);

            ASSERT_TRUE(summaries) << (summaries ? "" : summaries.error().context);
            ASSERT_EQ(summaries->size(), 1U);
            EXPECT_EQ(summaries->front().id, both);
            EXPECT_EQ(summaries->front().tags, (std::vector<std::string>{"long", "prose"}));
        }

        TYPED_TEST(TextLibraryRepositoryContract, TaggingTwiceIsOneTagAndUntaggingRemovesIt) {
            const core::TextId id = this->add(TestFixture::a_text("Prose", "hash-1"));

            ASSERT_TRUE(this->repository().tag(id, "prose"));
            ASSERT_TRUE(this->repository().tag(id, "prose"));
            const core::Result<std::vector<app::TextSummary>> twice = this->repository().list({});
            ASSERT_TRUE(twice);
            ASSERT_EQ(twice->size(), 1U);
            EXPECT_EQ(twice->front().tags.size(), 1U);

            ASSERT_TRUE(this->repository().untag(id, "prose"));
            const core::Result<std::vector<app::TextSummary>> after = this->repository().list({});
            ASSERT_TRUE(after);
            EXPECT_TRUE(after->front().tags.empty());
        }

        TYPED_TEST(TextLibraryRepositoryContract, ABookmarkIsUpdatedInPlace) {
            const core::TextId id = this->add(TestFixture::a_text("A book", "hash-1"));

            app::Bookmark first;
            first.text_id = id;
            first.offset = core::GraphemeIndex{100};
            first.updated_at = kNoon;
            ASSERT_TRUE(this->repository().set_bookmark(first));

            app::Bookmark later = first;
            later.offset = core::GraphemeIndex{250};
            later.updated_at = kNoon + core::Millis{60'000};
            ASSERT_TRUE(this->repository().set_bookmark(later));

            const core::Result<std::optional<app::Bookmark>> mark = this->repository().bookmark(id);
            ASSERT_TRUE(mark) << (mark ? "" : mark.error().context);
            ASSERT_TRUE(mark->has_value());
            EXPECT_EQ((*mark)->offset, core::GraphemeIndex{250});
        }

        TYPED_TEST(TextLibraryRepositoryContract, ATextWithNoBookmarkHasNone) {
            const core::TextId id = this->add(TestFixture::a_text("Prose", "hash-1"));

            const core::Result<std::optional<app::Bookmark>> mark = this->repository().bookmark(id);

            ASSERT_TRUE(mark) << (mark ? "" : mark.error().context);
            EXPECT_FALSE(mark->has_value());
        }

        TYPED_TEST(TextLibraryRepositoryContract, RemovingTakesTheTagsAndTheBookmark) {
            const core::TextId id = this->add(TestFixture::a_text("Prose", "hash-1"));
            ASSERT_TRUE(this->repository().tag(id, "prose"));
            app::Bookmark mark;
            mark.text_id = id;
            mark.offset = core::GraphemeIndex{10};
            mark.updated_at = kNoon;
            ASSERT_TRUE(this->repository().set_bookmark(mark));

            ASSERT_TRUE(this->repository().remove(id));

            EXPECT_TRUE(this->repository().list({}).value_or(std::vector<app::TextSummary>{}).empty());
            const core::Result<std::optional<app::Bookmark>> gone = this->repository().bookmark(id);
            ASSERT_TRUE(gone);
            EXPECT_FALSE(gone->has_value());
        }

        TYPED_TEST(TextLibraryRepositoryContract, RemovingSomethingAbsentIsNotAnError) {
            EXPECT_TRUE(this->repository().remove(core::TextId{999}));
        }

    }  // namespace
}  // namespace typeit
