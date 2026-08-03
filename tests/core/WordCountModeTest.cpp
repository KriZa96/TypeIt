#include <cstddef>
#include <gtest/gtest.h>
#include <string>
#include <variant>

#include "typeit/core/modes/IMode.h"
#include "typeit/core/modes/WordCountMode.h"
#include "typeit/core/util/Units.h"
#include "typeit/testing/ModeDriver.h"

namespace typeit::core {
    namespace {

        WordProgress progress_of(const WordCountMode& mode) { return std::get<WordProgress>(mode.progress()); }

        TEST(WordCountModeTest, FinishesAfterExactlyTheWordsAskedFor) {
            WordCountMode mode{3};
            testing::ModeDriver driver{"one two three four five", mode};

            driver.type("one two ");
            EXPECT_FALSE(mode.is_finished()) << "two words in";

            driver.type("three ");
            EXPECT_TRUE(mode.is_finished());
            EXPECT_EQ(progress_of(mode).done, 3U);
            EXPECT_EQ(progress_of(mode).total, 3U);
        }

        TEST(WordCountModeTest, AWordIsCommittedOnTheSpaceThatFollowsItNotOnItsLastLetter) {
            WordCountMode mode{1};
            testing::ModeDriver driver{"one two", mode};

            driver.type("one");
            EXPECT_FALSE(mode.is_finished()) << "the typist has not left the word yet";
            EXPECT_EQ(progress_of(mode).done, 0U);

            driver.type(" ");
            EXPECT_TRUE(mode.is_finished());
        }

        TEST(WordCountModeTest, ANewlineCommitsAWordJustAsASpaceDoes) {
            WordCountMode mode{1};
            testing::ModeDriver driver{"one\ntwo", mode};

            driver.type("one");
            ASSERT_FALSE(mode.is_finished());

            driver.type(" ");  // crosses the line break
            EXPECT_TRUE(mode.is_finished());
        }

        TEST(WordCountModeTest, TheLastWordOfTheTextCountsWithoutATrailingSpace) {
            WordCountMode mode{2};
            testing::ModeDriver driver{"one two", mode};

            driver.type("one two");

            EXPECT_TRUE(mode.is_finished()) << "there is no space left to type";
            EXPECT_EQ(progress_of(mode).done, 2U);
        }

        TEST(WordCountModeTest, SkippedWordsStillCount) {
            // A space mid-word abandons it. It was still a word, and the run is
            // still that many words long.
            WordCountMode mode{2};
            testing::ModeDriver driver{"alpha beta gamma", mode};

            driver.type("al ");
            EXPECT_EQ(progress_of(mode).done, 1U) << "alpha was left behind, badly";

            driver.type("be ");
            EXPECT_TRUE(mode.is_finished());
        }

        TEST(WordCountModeTest, ProgressCountsUpAsWordsAreCommitted) {
            WordCountMode mode{4};
            testing::ModeDriver driver{"one two three four", mode};

            EXPECT_EQ(progress_of(mode).done, 0U);
            driver.type("one ");
            EXPECT_EQ(progress_of(mode).done, 1U);
            driver.type("two ");
            EXPECT_EQ(progress_of(mode).done, 2U);
            driver.type("three ");
            EXPECT_EQ(progress_of(mode).done, 3U);
            driver.type("four");
            EXPECT_EQ(progress_of(mode).done, 4U);
        }

        TEST(WordCountModeTest, BackspacingOverTheLastSpaceGivesTheWordBackButNotTheRun) {
            // Progress follows the cursor, so it goes down. Finishing does not:
            // a run the typist has already been shown the results of does not
            // reopen.
            WordCountMode mode{2};
            testing::ModeDriver driver{"one two three", mode};

            driver.type("one two ");
            ASSERT_TRUE(mode.is_finished());

            driver.backspace();
            EXPECT_EQ(progress_of(mode).done, 1U);
            EXPECT_TRUE(mode.is_finished());
        }

        TEST(WordCountModeTest, TicksAloneDoNotCommitAnything) {
            WordCountMode mode{1};
            testing::ModeDriver driver{"one two", mode};

            driver.tick(100, Millis{1'000});

            EXPECT_FALSE(mode.is_finished());
            EXPECT_EQ(progress_of(mode).done, 0U);
        }

        TEST(WordCountModeTest, OneWordBehaves) {
            WordCountMode mode{1};
            testing::ModeDriver driver{"one two", mode};

            driver.type("one ");

            EXPECT_TRUE(mode.is_finished());
        }

        TEST(WordCountModeTest, AThousandWordsBehave) {
            std::string text;
            for (int i = 0; i < 1'000; ++i) {
                text += "word ";
            }
            WordCountMode mode{1'000};
            testing::ModeDriver driver{text, mode};

            driver.type(text.substr(0, text.size() - 6));
            EXPECT_FALSE(mode.is_finished()) << "999 words and a bit";

            driver.type("d ");
            EXPECT_TRUE(mode.is_finished());
            EXPECT_EQ(progress_of(mode).done, 1'000U);
        }

        TEST(WordCountModeTest, ARunLongerThanTheTextEndsWithTheText) {
            // Asking for more words than the text holds is a text-supply
            // problem (the provider streams more); the mode simply never
            // finishes, and says how far it got.
            WordCountMode mode{10};
            testing::ModeDriver driver{"one two", mode};

            driver.type("one two");

            EXPECT_FALSE(mode.is_finished());
            EXPECT_EQ(progress_of(mode).done, 2U);
        }

        TEST(WordCountModeDeathTest, ZeroWordsIsABug) { EXPECT_DEBUG_DEATH(WordCountMode{0}, "over before it begins"); }

    }  // namespace
}  // namespace typeit::core
