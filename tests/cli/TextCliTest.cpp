// The text library from a command line (TI-119).
//
// The formatting is what is tested here, over fakes. That the flags reach these
// functions at all is `CliParserTest`'s job, and that the binary exits with the
// right status is a ctest case beside the other smoke tests — three different
// questions, and running the process to answer the first would be the slowest
// way to ask it.

#include <algorithm>
#include <cstddef>
#include <gtest/gtest.h>
#include <string>
#include <vector>

#include "typeit/app/records/TextLibrary.h"
#include "typeit/app/services/TextLibraryService.h"
#include "typeit/cli/Cli.h"
#include "typeit/cli/Texts.h"
#include "typeit/core/util/Result.h"
#include "typeit/core/util/Units.h"
#include "typeit/testing/FakeClock.h"
#include "typeit/testing/Fakes.h"

namespace typeit::cli {
    namespace {

        using core::ErrorCode;

        constexpr core::Millis kNoon{1'767'225'600'000};

        class TextCliTest : public ::testing::Test {
        protected:
            /// A file in the fake filesystem, imported through the service.
            core::TextId import(const std::string& path, const std::string& contents) {
                files_.add_file(path, contents);
                CliOptions options;
                options.operand = path;
                const core::Result<std::string> said = import_text(service_, options);
                EXPECT_TRUE(said) << (said ? "" : said.error().context);
                const core::Result<std::vector<app::TextSummary>> texts = library_.list({});
                EXPECT_TRUE(texts);
                return (texts && !texts->empty()) ? texts->back().id : core::TextId{0};
            }

            testing::FakeTextLibraryRepository library_;
            testing::FakeFileSystem files_;
            testing::FakeClock clock_{kNoon};
            app::TextLibraryService service_{library_, files_, clock_};
        };

        // ---- import ------------------------------------------------------------

        TEST_F(TextCliTest, ImportReportsTheIdItStored) {
            files_.add_file("article.txt", "the quick brown fox jumps over the lazy dog");
            CliOptions options;
            options.operand = "article.txt";

            const core::Result<std::string> said = import_text(service_, options);

            ASSERT_TRUE(said) << (said ? "" : said.error().context);
            EXPECT_NE(said->find("article.txt"), std::string::npos) << *said;
            EXPECT_NE(said->find("imported as text 1"), std::string::npos) << *said;
        }

        TEST_F(TextCliTest, ImportingTheSameContentTwiceSaysSoRatherThanPretending) {
            // Somebody who adds the same article twice should learn that it was
            // already there, not that they now have two.
            files_.add_file("first.txt", "the quick brown fox");
            files_.add_file("second.txt", "the quick brown fox");
            CliOptions first;
            first.operand = "first.txt";
            CliOptions second;
            second.operand = "second.txt";

            ASSERT_TRUE(import_text(service_, first));
            const core::Result<std::string> said = import_text(service_, second);

            ASSERT_TRUE(said);
            EXPECT_NE(said->find("already in the library"), std::string::npos) << *said;
        }

        TEST_F(TextCliTest, ImportingSomethingUnreadableIsReportedRatherThanSaidToHaveWorked) {
            CliOptions options;
            options.operand = "nowhere.txt";

            const core::Result<std::string> said = import_text(service_, options);

            ASSERT_FALSE(said);
            EXPECT_EQ(said.error().code, ErrorCode::FileNotFound);
        }

        // ---- listing -----------------------------------------------------------

        TEST_F(TextCliTest, AnEmptyLibraryAnswersTheQuestionItWasAsked) {
            const core::Result<std::string> listed = list_texts(library_, service_, CliOptions{});

            ASSERT_TRUE(listed);
            EXPECT_NE(listed->find("No texts yet"), std::string::npos) << *listed;
            EXPECT_NE(listed->find("--import"), std::string::npos) << "and says what to do about it";
        }

        TEST_F(TextCliTest, TheListingIsTabSeparatedWithOneHeaderLine) {
            // `typeit --list-texts | awk` is how somebody finds the id to pass
            // to `--text-id`, so the columns must be findable without guessing
            // at runs of spaces.
            import("article.txt", "the quick brown fox jumps over the lazy dog");

            const core::Result<std::string> listed = list_texts(library_, service_, CliOptions{});

            ASSERT_TRUE(listed);
            EXPECT_TRUE(listed->starts_with("id\twords\tdifficulty\tprogress\ttitle\n")) << *listed;
            const std::size_t body = listed->find('\n') + 1;
            EXPECT_EQ(std::ranges::count(listed->substr(body), '\t'), 4) << *listed;
        }

        TEST_F(TextCliTest, OneTextIsOneLineEvenWhenItsTitleContainsATab) {
            // A title can contain anything; a listing somebody pipes into `cut`
            // cannot. One text is one line and one column is one field.
            files_.add_file("odd.txt", "the quick brown fox jumps over the lazy dog");
            CliOptions options;
            options.operand = "odd.txt";
            ASSERT_TRUE(service_.import_file("odd.txt", std::string{"a\ttitle\nwith breaks"}));

            const core::Result<std::string> listed = list_texts(library_, service_, CliOptions{});

            ASSERT_TRUE(listed);
            EXPECT_EQ(std::ranges::count(*listed, '\n'), 2) << "header and one row:\n" << *listed;
        }

        TEST_F(TextCliTest, TheListingShowsProgressAgainstTheBookmark) {
            const core::TextId id = import("book.txt", std::string(100, 'a'));
            ASSERT_TRUE(service_.advance(id, 50));

            const core::Result<std::string> listed = list_texts(library_, service_, CliOptions{});

            ASSERT_TRUE(listed);
            EXPECT_NE(listed->find("50%"), std::string::npos) << *listed;
        }

        TEST_F(TextCliTest, TheListingHonoursTheLimit) {
            import("one.txt", "the quick brown fox jumps");
            import("two.txt", "a wholly different sentence here");
            CliOptions options;
            options.last = 1;

            const core::Result<std::string> listed = list_texts(library_, service_, options);

            ASSERT_TRUE(listed);
            EXPECT_EQ(std::ranges::count(*listed, '\n'), 2) << "header and one row:\n" << *listed;
        }

        // ---- removal -----------------------------------------------------------

        TEST_F(TextCliTest, RemovingWithoutYesIsRefusedAndSaysWhichFlagToPass) {
            // Refused rather than prompted: a prompt on stdin is one a pipe
            // cannot answer, and stdin here may well be a piped text.
            const core::TextId id = import("book.txt", "the quick brown fox jumps");
            CliOptions options;
            options.remove_id = id;

            const core::Result<std::string> said = remove_text(library_, options);

            ASSERT_FALSE(said);
            EXPECT_EQ(said.error().code, ErrorCode::InvalidArgument);
            EXPECT_NE(said.error().context.find("--yes"), std::string::npos) << said.error().context;
            EXPECT_TRUE(library_.get(id).value().has_value()) << "and nothing was removed";
        }

        TEST_F(TextCliTest, RemovingWithYesSaysWhatElseWent) {
            // "Removed" alone leaves somebody wondering about the runs they
            // typed from it.
            const core::TextId id = import("book.txt", "the quick brown fox jumps");
            CliOptions options;
            options.remove_id = id;
            options.assume_yes = true;

            const core::Result<std::string> said = remove_text(library_, options);

            ASSERT_TRUE(said) << (said ? "" : said.error().context);
            EXPECT_NE(said->find("tags and bookmark"), std::string::npos) << *said;
            EXPECT_NE(said->find("sessions are kept"), std::string::npos) << *said;
            EXPECT_FALSE(library_.get(id).value().has_value());
        }

        TEST_F(TextCliTest, RemovingAnIdNobodyStoredIsAClearError) {
            CliOptions options;
            options.remove_id = core::TextId{404};
            options.assume_yes = true;

            const core::Result<std::string> said = remove_text(library_, options);

            ASSERT_FALSE(said);
            EXPECT_EQ(said.error().code, ErrorCode::FileNotFound);
            EXPECT_NE(said.error().context.find("404"), std::string::npos) << said.error().context;
        }

        TEST_F(TextCliTest, RemovingWithNoIdAtAllIsAMisuse) {
            const core::Result<std::string> said = remove_text(library_, CliOptions{});

            ASSERT_FALSE(said);
            EXPECT_EQ(said.error().code, ErrorCode::InvalidArgument);
        }

    }  // namespace
}  // namespace typeit::cli
