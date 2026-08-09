// A folder at a time (TX-004).
//
// The rule the whole thing turns on: a batch reports rather than fails. One
// unreadable file in a folder of two hundred should not cost somebody the other
// hundred and ninety-nine, and a summary they can read beats an error naming
// only the first thing that went wrong.

#include <gtest/gtest.h>
#include <optional>
#include <string>

#include "typeit/app/services/TextLibraryService.h"
#include "typeit/core/util/Result.h"
#include "typeit/testing/FakeClock.h"
#include "typeit/testing/Fakes.h"

namespace typeit::app {
    namespace {

        /// A service over fakes, with the files a case wants already in place.
        class DirectoryImportTest : public ::testing::Test {
        protected:
            testing::FakeTextLibraryRepository library;
            testing::FakeFileSystem files;
            testing::FakeClock clock{core::Millis{1'700'000'000'000}};
            TextLibraryService service{library, files, clock};
        };

        TEST_F(DirectoryImportTest, EveryFileInTheFolderBecomesItsOwnText) {
            files.add_file("/texts/one.txt", "The first text, which is long enough to import.");
            files.add_file("/texts/two.txt", "The second text, which is also long enough.");

            const core::Result<DirectoryImport> summary = service.import_directory("/texts");

            ASSERT_TRUE(summary) << (summary ? "" : summary.error().context);
            EXPECT_EQ(summary->imported.size(), 2U);
            EXPECT_TRUE(summary->skipped.empty());
        }

        TEST_F(DirectoryImportTest, AnUnreadableFileIsSkippedAndTheRestStillImport) {
            files.add_file("/texts/good.txt", "A text that is perfectly fine.");
            files.add_file("/texts/empty.txt", "");

            const core::Result<DirectoryImport> summary = service.import_directory("/texts");

            ASSERT_TRUE(summary);
            EXPECT_EQ(summary->imported.size(), 1U);
            ASSERT_EQ(summary->skipped.size(), 1U);
            EXPECT_NE(summary->skipped.front().find("empty.txt"), std::string::npos) << summary->skipped.front();
        }

        TEST_F(DirectoryImportTest, TheReasonTravelsWithTheName) {
            // A count of failures tells somebody that something is wrong
            // without telling them what.
            files.add_file("/texts/good.txt", "A text that is perfectly fine.");
            files.add_file("/texts/empty.txt", "");

            const core::Result<DirectoryImport> summary = service.import_directory("/texts");

            ASSERT_TRUE(summary);
            ASSERT_EQ(summary->skipped.size(), 1U);
            EXPECT_NE(summary->skipped.front().find("nothing here to type"), std::string::npos)
                    << summary->skipped.front();
        }

        TEST_F(DirectoryImportTest, SubfoldersAreNotRecursedIntoAndSayWhy) {
            // Recursing would import a source tree's vendored dependencies from
            // one keystroke.
            files.add_file("/texts/one.txt", "A text that is perfectly fine.");
            files.add_directory("/texts/deeper");

            const core::Result<DirectoryImport> summary = service.import_directory("/texts");

            ASSERT_TRUE(summary);
            EXPECT_EQ(summary->imported.size(), 1U);
            ASSERT_EQ(summary->skipped.size(), 1U);
            EXPECT_NE(summary->skipped.front().find("deeper"), std::string::npos) << summary->skipped.front();
            EXPECT_NE(summary->skipped.front().find("folder"), std::string::npos) << summary->skipped.front();
        }

        TEST_F(DirectoryImportTest, AnEmptyFolderSaysSoRatherThanSucceedingAtNothing) {
            files.add_directory("/texts");

            const core::Result<DirectoryImport> summary = service.import_directory("/texts");

            ASSERT_FALSE(summary);
            EXPECT_EQ(summary.error().code, core::ErrorCode::EmptyText);
            EXPECT_NE(summary.error().context.find("nothing in this folder"), std::string::npos)
                    << summary.error().context;
        }

        TEST_F(DirectoryImportTest, AFileAndAMissingPathAreDifferentMistakes) {
            // "Not a directory" about a path that is not there sends somebody
            // looking for the wrong mistake.
            files.add_file("/texts/one.txt", "A text.");

            const core::Result<DirectoryImport> file = service.import_directory("/texts/one.txt");
            const core::Result<DirectoryImport> missing = service.import_directory("/nowhere");

            ASSERT_FALSE(file);
            ASSERT_FALSE(missing);
            EXPECT_EQ(file.error().code, core::ErrorCode::FileUnreadable);
            EXPECT_EQ(missing.error().code, core::ErrorCode::FileNotFound);
        }

        TEST_F(DirectoryImportTest, TheSameTextTwiceIsImportedOnceAndSaidSo) {
            // Deduplication is by content, so a folder holding two copies of
            // one file reports two imports pointing at one text rather than
            // silently storing it twice.
            files.add_file("/texts/one.txt", "The very same words in both files.");
            files.add_file("/texts/copy.txt", "The very same words in both files.");

            const core::Result<DirectoryImport> summary = service.import_directory("/texts");

            ASSERT_TRUE(summary);
            ASSERT_EQ(summary->imported.size(), 2U);
            EXPECT_EQ(summary->imported[0].id, summary->imported[1].id);
            EXPECT_TRUE(summary->imported[1].already_present);
        }

        TEST_F(DirectoryImportTest, EachFileGoesThroughItsOwnExtractor) {
            // The point of importing a folder: the Markdown file arrives
            // stripped, the source file arrives with its indentation, and
            // nobody had to say which was which.
            // Listed in a deterministic order by the port, so `main.py` comes
            // before `notes.md` and the case can name them positionally.
            files.add_file("/texts/notes.md", "# A heading\n\nSome *emphasised* prose to type.\n");
            files.add_file("/texts/main.py", "def main():\n    return 1\n");

            const core::Result<DirectoryImport> summary = service.import_directory("/texts");

            ASSERT_TRUE(summary);
            ASSERT_EQ(summary->imported.size(), 2U);
            const core::Result<std::optional<TextItem>> code = library.get(summary->imported[0].id);
            const core::Result<std::optional<TextItem>> markdown = library.get(summary->imported[1].id);
            ASSERT_TRUE(code);
            ASSERT_TRUE(markdown);
            ASSERT_TRUE(code->has_value());
            ASSERT_TRUE(markdown->has_value());
            EXPECT_EQ((*markdown)->content.find('*'), std::string::npos)
                    << "the emphasis went: " << (*markdown)->content;
            EXPECT_NE((*code)->content.find("    return 1"), std::string::npos)
                    << "and the indentation stayed: " << (*code)->content;
        }

    }  // namespace
}  // namespace typeit::app
