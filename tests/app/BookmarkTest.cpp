// Typing a book across many sessions (TI-115, GAMEPLAY §2.3).
//
// The bookmark is the only thing standing between "I am reading Moby Dick at
// forty words a minute" and "I retype the first page every evening", so the
// arithmetic around it is worth being exact about — particularly at the two
// ends, where a percentage that is nearly right is a percentage that lies.

#include <cstddef>
#include <gtest/gtest.h>
#include <optional>
#include <string>

#include "typeit/app/records/TextLibrary.h"
#include "typeit/app/services/TextLibraryService.h"
#include "typeit/core/util/Result.h"
#include "typeit/core/util/Units.h"
#include "typeit/testing/FakeClock.h"
#include "typeit/testing/Fakes.h"

namespace typeit::app {
    namespace {

        using core::ErrorCode;

        constexpr core::Millis kNoon{1'767'225'600'000};

        class BookmarkTest : public ::testing::Test {
        protected:
            /// A text of exactly `graphemes` characters, imported.
            core::TextId a_text(std::size_t graphemes) {
                files_.add_file("book.txt", std::string(graphemes, 'a'));
                const core::Result<ImportOutcome> imported = service_.import_file("book.txt");
                EXPECT_TRUE(imported) << (imported ? "" : imported.error().context);
                return imported ? imported->id : core::TextId{0};
            }

            [[nodiscard]] TextProgress progress_of(core::TextId id) {
                const core::Result<TextProgress> progress = service_.progress(id);
                EXPECT_TRUE(progress) << (progress ? "" : progress.error().context);
                return progress.value_or(TextProgress{});
            }

            testing::FakeTextLibraryRepository library_;
            testing::FakeFileSystem files_;
            testing::FakeClock clock_{kNoon};
            TextLibraryService service_{library_, files_, clock_};
        };

        TEST_F(BookmarkTest, ATextNobodyHasStartedIsAtZero) {
            // Not an error: not having started is the normal state of most of a
            // library, and a screen listing it should not have to special-case
            // every text somebody has not opened.
            const core::TextId id = a_text(100);

            const TextProgress progress = progress_of(id);

            EXPECT_EQ(progress.offset, core::GraphemeIndex{0});
            EXPECT_EQ(progress.total, 100U);
            EXPECT_DOUBLE_EQ(progress.fraction, 0.0) << "exactly zero, not nearly";
            EXPECT_FALSE(progress.finished);
        }

        TEST_F(BookmarkTest, CompletingAChunkAdvancesTheBookmarkByExactlyThatChunk) {
            const core::TextId id = a_text(1'000);

            ASSERT_TRUE(service_.advance(id, 250));

            const TextProgress progress = progress_of(id);
            EXPECT_EQ(progress.offset, core::GraphemeIndex{250});
            EXPECT_DOUBLE_EQ(progress.fraction, 0.25);
        }

        TEST_F(BookmarkTest, ChunksAccumulateAcrossSessions) {
            // The whole feature: four evenings and the book is done.
            const core::TextId id = a_text(1'000);

            for (int evening = 0; evening < 4; ++evening) {
                ASSERT_TRUE(service_.advance(id, 250));
            }

            const TextProgress progress = progress_of(id);
            EXPECT_EQ(progress.offset, core::GraphemeIndex{1'000});
            EXPECT_TRUE(progress.finished);
        }

        TEST_F(BookmarkTest, AnAbandonedSessionAdvancesByWhatWasActuallyTyped) {
            // Not by the chunk that was offered. Advancing by the whole chunk
            // would skip text nobody saw, which is the one failure that makes
            // the feature worse than not having it.
            const core::TextId id = a_text(1'000);

            ASSERT_TRUE(service_.advance(id, 137));

            EXPECT_EQ(progress_of(id).offset, core::GraphemeIndex{137});
        }

        TEST_F(BookmarkTest, ReachingTheEndIsExactlyOneHundredPercent) {
            const core::TextId id = a_text(500);

            ASSERT_TRUE(service_.advance(id, 500));

            const TextProgress progress = progress_of(id);
            EXPECT_DOUBLE_EQ(progress.fraction, 1.0) << "exactly one, not 0.998";
            EXPECT_TRUE(progress.finished);
        }

        TEST_F(BookmarkTest, OverCountingIsClampedRatherThanStoredPastTheEnd) {
            // A caller that over-counts must not leave a bookmark pointing past
            // the last grapheme, which every reader of it would then have to
            // defend against.
            const core::TextId id = a_text(500);

            ASSERT_TRUE(service_.advance(id, 9'999));

            const TextProgress progress = progress_of(id);
            EXPECT_EQ(progress.offset, core::GraphemeIndex{500});
            EXPECT_DOUBLE_EQ(progress.fraction, 1.0);
            EXPECT_TRUE(progress.finished);
        }

        TEST_F(BookmarkTest, ResettingReturnsToTheBeginning) {
            const core::TextId id = a_text(500);
            ASSERT_TRUE(service_.advance(id, 500));
            ASSERT_TRUE(progress_of(id).finished);

            ASSERT_TRUE(service_.reset_progress(id));

            const TextProgress progress = progress_of(id);
            EXPECT_EQ(progress.offset, core::GraphemeIndex{0});
            EXPECT_DOUBLE_EQ(progress.fraction, 0.0);
            EXPECT_FALSE(progress.finished);
        }

        TEST_F(BookmarkTest, ThePercentageIsExactAtBothEndsAndInTheMiddle) {
            const core::TextId id = a_text(4);

            EXPECT_DOUBLE_EQ(progress_of(id).fraction, 0.0);
            ASSERT_TRUE(service_.advance(id, 2));
            EXPECT_DOUBLE_EQ(progress_of(id).fraction, 0.5);
            ASSERT_TRUE(service_.advance(id, 2));
            EXPECT_DOUBLE_EQ(progress_of(id).fraction, 1.0);
        }

        TEST_F(BookmarkTest, ReImportingAChangedTextStartsAtZeroRatherThanResumingIntoShiftedContent) {
            // A changed file is a different text — deduplication is by content
            // — so the bookmark stays with the version it was made against and
            // the new one starts at nothing. Silently resuming at an offset
            // computed against text that has moved would drop somebody into the
            // middle of a sentence they never read.
            files_.add_file("draft.txt", std::string(1'000, 'a'));
            const core::Result<ImportOutcome> first = service_.import_file("draft.txt");
            ASSERT_TRUE(first);
            ASSERT_TRUE(service_.advance(first->id, 400));

            files_.add_file("draft.txt", "a rewritten draft, entirely different words");
            const core::Result<ImportOutcome> second = service_.import_file("draft.txt");

            ASSERT_TRUE(second);
            ASSERT_NE(second->id, first->id) << "different content is a different text";
            EXPECT_EQ(progress_of(second->id).offset, core::GraphemeIndex{0});
            EXPECT_EQ(progress_of(first->id).offset, core::GraphemeIndex{400})
                    << "and the old bookmark still belongs to the old version";
        }

        TEST_F(BookmarkTest, ReImportingTheSameTextKeepsItsPlace) {
            // The other half. Importing a file twice is one text, so the
            // bookmark survives — otherwise re-adding a book would silently
            // throw away a month of evenings.
            files_.add_file("book.txt", std::string(1'000, 'a'));
            const core::Result<ImportOutcome> first = service_.import_file("book.txt");
            ASSERT_TRUE(first);
            ASSERT_TRUE(service_.advance(first->id, 400));

            const core::Result<ImportOutcome> again = service_.import_file("book.txt");

            ASSERT_TRUE(again);
            EXPECT_TRUE(again->already_present);
            EXPECT_EQ(progress_of(again->id).offset, core::GraphemeIndex{400});
        }

        TEST_F(BookmarkTest, ProgressForATextNobodyImportedIsAnError) {
            // A stale id from a deleted text. Reported rather than answered
            // with a confident zero, which reads as "not started" and is not.
            const core::Result<TextProgress> progress = service_.progress(core::TextId{404});

            ASSERT_FALSE(progress);
            EXPECT_EQ(progress.error().code, ErrorCode::FileNotFound);
        }

    }  // namespace
}  // namespace typeit::app
