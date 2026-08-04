#include <gtest/gtest.h>
#include <string_view>
#include <variant>

#include "typeit/core/modes/IMode.h"
#include "typeit/core/modes/QuoteMode.h"
#include "typeit/core/util/Units.h"
#include "typeit/testing/ModeDriver.h"

namespace typeit::core {
    namespace {

        TextProgress progress_of(const QuoteMode& mode) { return std::get<TextProgress>(mode.progress()); }

        TEST(QuoteModeTest, IsRegisteredAndPersistedAsQuote) {
            // The id goes into the database and into the config file, so a typo
            // in it is a silent break in every saved run of this mode.
            const QuoteMode mode;

            EXPECT_EQ(mode.id(), "quote");
        }

        TEST(QuoteModeTest, FinishesAtTheLastGraphemeOfTheText) {
            QuoteMode mode;
            testing::ModeDriver driver{"one two", mode};

            driver.type("one tw");
            EXPECT_FALSE(mode.is_finished());

            driver.type("o");
            EXPECT_TRUE(mode.is_finished());
        }

        // Ported from InputLineTest.FinishGameOnFullInput.
        TEST(QuoteModeTest, LegacyFinishGameOnFullInput) {
            constexpr std::string_view text = "line1\nline 2\nThird line\nsecond last line\nfinally last line.";
            QuoteMode mode;
            testing::ModeDriver driver{text, mode};

            driver.type("line1 line 2 Third line second last line finally last line.");

            EXPECT_TRUE(mode.is_finished());
        }

        TEST(QuoteModeTest, DoesNotFinishEarlyWhenTheTypistRunsPastTheEnd) {
            // Typing past the end changes nothing in the model, so there is
            // nothing here to be confused by either.
            QuoteMode mode;
            testing::ModeDriver driver{"ab", mode};

            driver.type("ab");
            ASSERT_TRUE(mode.is_finished());

            driver.type("cdefgh");

            EXPECT_TRUE(mode.is_finished());
            EXPECT_EQ(progress_of(mode).position, GraphemeIndex{2});
            EXPECT_EQ(progress_of(mode).total, 2U);
        }

        TEST(QuoteModeTest, AnEmptyTextIsFinishedBeforeAnythingHappens) {
            // The legacy engine survives this only because should_finish_game()
            // happens to return early. Here it is the stated behaviour, and
            // there is nothing to read past the end of.
            QuoteMode mode;
            const testing::ModeDriver driver{"", mode};

            EXPECT_TRUE(mode.is_finished());
            EXPECT_EQ(mode.completion(), 1.0);
            EXPECT_EQ(progress_of(mode).total, 0U);
        }

        TEST(QuoteModeTest, CompletionIsExactAtTheEnds) {
            QuoteMode mode;
            testing::ModeDriver driver{"abcd", mode};

            EXPECT_EQ(mode.completion(), 0.0) << "0%";

            driver.type("ab");
            EXPECT_EQ(mode.completion(), 0.5) << "50%";

            driver.type("cd");
            EXPECT_EQ(mode.completion(), 1.0) << "100%";
        }

        TEST(QuoteModeTest, CompletionCountsGraphemesNotBytes) {
            QuoteMode mode;
            testing::ModeDriver driver{"čšžđ", mode};

            driver.type("čš");

            EXPECT_EQ(mode.completion(), 0.5) << "two of four clusters, not four of nine bytes";
        }

        TEST(QuoteModeTest, BackspacingOffTheEndDoesNotReopenTheRun) {
            QuoteMode mode;
            testing::ModeDriver driver{"ab", mode};

            driver.type("ab");
            ASSERT_TRUE(mode.is_finished());

            driver.backspace();

            EXPECT_TRUE(mode.is_finished());
            EXPECT_EQ(progress_of(mode).position, GraphemeIndex{1}) << "progress is honest about where the cursor is";
        }

        TEST(QuoteModeTest, WrongTypingStillFinishesTheRun) {
            // Finishing is about reaching the end, not about getting there
            // correctly. What was got wrong is the metrics' business.
            QuoteMode mode;
            testing::ModeDriver driver{"abc", mode};

            driver.type("xyz");

            EXPECT_TRUE(mode.is_finished());
        }

        TEST(QuoteModeTest, TicksAloneNeverFinishAText) {
            QuoteMode mode;
            testing::ModeDriver driver{"abc", mode};

            driver.tick(100, Millis{1'000});

            EXPECT_FALSE(mode.is_finished());
        }

    }  // namespace
}  // namespace typeit::core
