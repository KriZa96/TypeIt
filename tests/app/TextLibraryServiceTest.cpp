// Importing a text (TI-070).

#include <cstddef>
#include <gtest/gtest.h>
#include <optional>
#include <string>

#include "typeit/app/records/TextLibrary.h"
#include "typeit/app/services/TextLibraryService.h"
#include "typeit/core/text/TextNormalizer.h"
#include "typeit/core/util/Result.h"
#include "typeit/core/util/Sha256.h"
#include "typeit/core/util/Units.h"
#include "typeit/testing/FakeClock.h"
#include "typeit/testing/Fakes.h"

namespace typeit::app {
    namespace {

        using core::ErrorCode;
        using core::Status;

        constexpr core::Millis kNoon{1'767'225'600'000};

        class TextLibraryServiceTest : public ::testing::Test {
        protected:
            core::Result<ImportOutcome> import_file(const std::string& path,
                                                    std::optional<std::string> title = std::nullopt) {
                return service_.import_file(path, std::move(title));
            }

            testing::FakeTextLibraryRepository library_;
            testing::FakeFileSystem files_;
            testing::FakeClock clock_{kNoon};
            TextLibraryService service_{library_, files_, clock_};
        };

        // ---- what gets stored ----------------------------------------------

        TEST_F(TextLibraryServiceTest, ImportingStoresTheNormalisedContentAndKeepsTheRaw) {
            files_.add_file("/home/kim/prose.txt", "The  “quick”  fox\r\n");

            const core::Result<ImportOutcome> imported = import_file("/home/kim/prose.txt");

            ASSERT_TRUE(imported) << (imported ? "" : imported.error().context);
            const core::Result<std::optional<TextItem>> stored = library_.get(imported->id);
            ASSERT_TRUE(stored);
            ASSERT_TRUE(stored->has_value());
            EXPECT_EQ((*stored)->content, "The \"quick\" fox\n") << "what the typist is scored against";
            ASSERT_TRUE((*stored)->content_raw.has_value());
            EXPECT_EQ(*(*stored)->content_raw, "The  “quick”  fox\r\n")
                    << "so the settings can change later without re-importing";
        }

        TEST_F(TextLibraryServiceTest, TextThatNormalisationDidNotTouchIsNotStoredTwice) {
            files_.add_file("/home/kim/plain.txt", "the quick brown fox");

            const core::Result<ImportOutcome> imported = import_file("/home/kim/plain.txt");

            ASSERT_TRUE(imported);
            const core::Result<std::optional<TextItem>> stored = library_.get(imported->id);
            ASSERT_TRUE(stored);
            EXPECT_FALSE((*stored)->content_raw.has_value()) << "a byte-identical copy answers nothing";
        }

        TEST_F(TextLibraryServiceTest, TheHashIsOfTheNormalisedContent) {
            files_.add_file("/home/kim/prose.txt", "the quick brown fox\r\n");

            const core::Result<ImportOutcome> imported = import_file("/home/kim/prose.txt");

            ASSERT_TRUE(imported);
            const core::Result<std::optional<TextItem>> stored = library_.get(imported->id);
            ASSERT_TRUE(stored);
            EXPECT_EQ((*stored)->content_sha256, core::sha256_hex("the quick brown fox\n"));
        }

        TEST_F(TextLibraryServiceTest, TheCountsAndTheScoreAreRecorded) {
            files_.add_file("/home/kim/prose.txt", "the quick brown fox");

            const core::Result<ImportOutcome> imported = import_file("/home/kim/prose.txt");

            ASSERT_TRUE(imported);
            const core::Result<std::optional<TextItem>> stored = library_.get(imported->id);
            ASSERT_TRUE(stored);
            EXPECT_EQ((*stored)->word_count, 4U);
            EXPECT_EQ((*stored)->grapheme_count, 19U);
            ASSERT_TRUE((*stored)->difficulty.has_value());
            EXPECT_GE(*(*stored)->difficulty, 1.0);
            EXPECT_LE(*(*stored)->difficulty, 10.0);
            EXPECT_EQ((*stored)->source, TextSource::File);
            EXPECT_EQ((*stored)->created_at, kNoon) << "from the injected clock, not from the machine's";
        }

        // ---- deduplication --------------------------------------------------

        TEST_F(TextLibraryServiceTest, TheSameContentTwiceIsOneText) {
            files_.add_file("/home/kim/prose.txt", "the quick brown fox");
            files_.add_file("/home/kim/copy.txt", "the quick brown fox");

            const core::Result<ImportOutcome> first = import_file("/home/kim/prose.txt");
            const core::Result<ImportOutcome> second = import_file("/home/kim/copy.txt");

            ASSERT_TRUE(first);
            ASSERT_TRUE(second) << (second ? "" : second.error().context);
            EXPECT_FALSE(first->already_present);
            EXPECT_TRUE(second->already_present) << "and the caller is told rather than left to guess";
            EXPECT_EQ(second->id, first->id);
            EXPECT_EQ(library_.list({}).value_or(std::vector<TextSummary>{}).size(), 1U) << "one row, not two";
        }

        TEST_F(TextLibraryServiceTest, TheSameFileWithChangedContentIsANewText) {
            files_.add_file("/home/kim/notes.txt", "the first draft");
            const core::Result<ImportOutcome> first = import_file("/home/kim/notes.txt");
            ASSERT_TRUE(first);

            files_.add_file("/home/kim/notes.txt", "the second draft, which says more");
            const core::Result<ImportOutcome> second = import_file("/home/kim/notes.txt");

            ASSERT_TRUE(second) << (second ? "" : second.error().context);
            EXPECT_FALSE(second->already_present);
            EXPECT_NE(second->id, first->id);
            EXPECT_EQ(library_.list({}).value_or(std::vector<TextSummary>{}).size(), 2U);
        }

        TEST_F(TextLibraryServiceTest, NormalisationIsWhatDecidesWhetherTwoFilesAreTheSame) {
            // Same text, saved on Windows and on Linux. One import, because the
            // line endings are not part of what anyone types.
            files_.add_file("/home/kim/windows.txt", "the quick brown fox\r\n");
            files_.add_file("/home/kim/linux.txt", "the quick brown fox\n");

            ASSERT_TRUE(import_file("/home/kim/windows.txt"));
            const core::Result<ImportOutcome> second = import_file("/home/kim/linux.txt");

            ASSERT_TRUE(second);
            EXPECT_TRUE(second->already_present);
        }

        // ---- refusals -------------------------------------------------------

        TEST_F(TextLibraryServiceTest, InvalidUtf8IsRefusedWithTheByteOffsetAndNothingIsStored) {
            files_.add_file("/home/kim/broken.txt", "fine\xE6\xBC");

            const core::Result<ImportOutcome> imported = import_file("/home/kim/broken.txt");

            ASSERT_FALSE(imported);
            EXPECT_EQ(imported.error().code, ErrorCode::InvalidUtf8);
            EXPECT_TRUE(imported.error().context.starts_with("byte 4:")) << imported.error().context;
            EXPECT_EQ(library_.adds, 0U);
        }

        TEST_F(TextLibraryServiceTest, AnEmptyFileIsRefusedWithSomethingToRead) {
            // Defect C1: 1.0 accepted this and then read past the end of it.
            files_.add_file("/home/kim/empty.txt", "");

            const core::Result<ImportOutcome> imported = import_file("/home/kim/empty.txt");

            ASSERT_FALSE(imported);
            EXPECT_EQ(imported.error().code, ErrorCode::EmptyText);
            EXPECT_NE(imported.error().context.find("/home/kim/empty.txt"), std::string::npos)
                    << imported.error().context;
            EXPECT_EQ(library_.adds, 0U);
        }

        TEST_F(TextLibraryServiceTest, AFileOfNothingButWhitespaceIsAlsoEmpty) {
            files_.add_file("/home/kim/blank.txt", "   \n\t\n  \r\n");

            const core::Result<ImportOutcome> imported = import_file("/home/kim/blank.txt");

            ASSERT_FALSE(imported);
            EXPECT_EQ(imported.error().code, ErrorCode::EmptyText);
        }

        TEST_F(TextLibraryServiceTest, AFileOverTheLimitIsRefusedWithTheLimitNamed) {
            files_.add_file("/home/kim/huge.txt", std::string(TextLibraryService::kMaxImportBytes + 1, 'a'));

            const core::Result<ImportOutcome> imported = import_file("/home/kim/huge.txt");

            ASSERT_FALSE(imported);
            EXPECT_EQ(imported.error().code, ErrorCode::TextTooLarge);
            EXPECT_NE(imported.error().context.find(std::to_string(TextLibraryService::kMaxImportBytes)),
                      std::string::npos)
                    << imported.error().context;
            EXPECT_EQ(library_.adds, 0U);
        }

        TEST_F(TextLibraryServiceTest, AMissingFileIsFileNotFound) {
            const core::Result<ImportOutcome> imported = import_file("/home/kim/gone.txt");

            ASSERT_FALSE(imported);
            EXPECT_EQ(imported.error().code, ErrorCode::FileNotFound) << "not a crash, and not a generic failure";
        }

        TEST_F(TextLibraryServiceTest, ADirectoryIsUnreadableRatherThanMissing) {
            files_.add_directory("/home/kim/corpus");

            const core::Result<ImportOutcome> imported = import_file("/home/kim/corpus");

            ASSERT_FALSE(imported);
            EXPECT_EQ(imported.error().code, ErrorCode::FileUnreadable)
                    << "the person who typed the path needs to know which";
        }

        TEST_F(TextLibraryServiceTest, AFailureToStoreIsReportedRatherThanSwallowed) {
            files_.add_file("/home/kim/prose.txt", "the quick brown fox");
            library_.failure.fail_next(core::make_error(ErrorCode::DbQuery, "the disk is full"));

            const core::Result<ImportOutcome> imported = import_file("/home/kim/prose.txt");

            ASSERT_FALSE(imported);
            EXPECT_EQ(imported.error().code, ErrorCode::DbQuery);
        }

        // ---- titles ---------------------------------------------------------

        TEST_F(TextLibraryServiceTest, AFileIsTitledAfterItsName) {
            files_.add_file("/home/kim/The Rust Book.txt", "the quick brown fox");

            const core::Result<ImportOutcome> imported = import_file("/home/kim/The Rust Book.txt");

            ASSERT_TRUE(imported);
            EXPECT_EQ(library_.get(imported->id).value()->title, "The Rust Book") << "without the extension";
        }

        TEST_F(TextLibraryServiceTest, AGivenTitleWins) {
            files_.add_file("/home/kim/tmp1234.txt", "the quick brown fox");

            const core::Result<ImportOutcome> imported = import_file("/home/kim/tmp1234.txt", "Chapter One");

            ASSERT_TRUE(imported);
            EXPECT_EQ(library_.get(imported->id).value()->title, "Chapter One");
        }

        TEST_F(TextLibraryServiceTest, APasteIsTitledAfterItsOpeningWords) {
            // A library of things called "Untitled" is not a library.
            const core::Result<ImportOutcome> imported = service_.import_text(
                    "the quick brown fox jumps over the lazy dog and keeps going", TextSource::Paste);

            ASSERT_TRUE(imported) << (imported ? "" : imported.error().context);
            const std::string title = library_.get(imported->id).value()->title;
            EXPECT_TRUE(title.starts_with("the quick brown fox")) << title;
            EXPECT_TRUE(title.ends_with("...")) << "and says it was cut short: " << title;
        }

        TEST_F(TextLibraryServiceTest, APasteRecordsThatItWasAPaste) {
            const core::Result<ImportOutcome> imported = service_.import_text("the quick brown fox", TextSource::Stdin);

            ASSERT_TRUE(imported);
            const core::Result<std::optional<TextItem>> stored = library_.get(imported->id);
            ASSERT_TRUE(stored);
            EXPECT_EQ((*stored)->source, TextSource::Stdin);
            EXPECT_FALSE((*stored)->origin.has_value()) << "there is no path to record";
        }

        // ---- normalisation is configurable ----------------------------------

        TEST_F(TextLibraryServiceTest, TheNormalisationSettingsAreTheOnesImportUses) {
            core::NormalizeOptions simplify;
            simplify.strip_punctuation = true;
            simplify.lowercase = true;
            service_.set_normalization(simplify);
            files_.add_file("/home/kim/prose.txt", "The Quick, Brown Fox.");

            const core::Result<ImportOutcome> imported = import_file("/home/kim/prose.txt");

            ASSERT_TRUE(imported);
            EXPECT_EQ(library_.get(imported->id).value()->content, "the quick brown fox");
        }

        // ---- bookmarks -------------------------------------------------------

        TEST_F(TextLibraryServiceTest, BookmarkingStampsTheClockRatherThanTrustingTheCaller) {
            files_.add_file("/home/kim/book.txt", "the quick brown fox");
            const core::Result<ImportOutcome> imported = import_file("/home/kim/book.txt");
            ASSERT_TRUE(imported);
            clock_.advance(core::Millis{60'000});

            const Status marked = service_.bookmark(imported->id, core::GraphemeIndex{12});

            ASSERT_TRUE(marked) << (marked ? "" : marked.error().context);
            const core::Result<std::optional<Bookmark>> mark = library_.bookmark(imported->id);
            ASSERT_TRUE(mark);
            ASSERT_TRUE(mark->has_value());
            EXPECT_EQ((*mark)->offset, core::GraphemeIndex{12});
            EXPECT_EQ((*mark)->updated_at, kNoon + core::Millis{60'000});
        }

        TEST_F(TextLibraryServiceTest, ASecondBookmarkReplacesTheFirst) {
            files_.add_file("/home/kim/book.txt", "the quick brown fox");
            const core::Result<ImportOutcome> imported = import_file("/home/kim/book.txt");
            ASSERT_TRUE(imported);

            ASSERT_TRUE(service_.bookmark(imported->id, core::GraphemeIndex{4}));
            ASSERT_TRUE(service_.bookmark(imported->id, core::GraphemeIndex{12}));

            const core::Result<std::optional<Bookmark>> mark = library_.bookmark(imported->id);
            ASSERT_TRUE(mark);
            ASSERT_TRUE(mark->has_value());
            EXPECT_EQ((*mark)->offset, core::GraphemeIndex{12}) << "two bookmarks in one book is not a question";
        }

    }  // namespace
}  // namespace typeit::app
