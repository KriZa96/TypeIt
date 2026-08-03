#include <cstddef>
#include <cstdint>
#include <gtest/gtest.h>
#include <random>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "typeit/core/session/Keystroke.h"
#include "typeit/core/session/KeystrokeLog.h"
#include "typeit/core/session/TypingModel.h"
#include "typeit/core/text/Grapheme.h"
#include "typeit/core/text/TextBuffer.h"
#include "typeit/core/util/Result.h"
#include "typeit/core/util/Units.h"
#include "typeit/testing/TypingRun.h"

namespace typeit::core {
    namespace {

        TEST(TypingModelTest, ACorrectGraphemeAdvancesTheCursorAndLogsTheEvent) {
            testing::TypingRun typing("abc");
            typing.type("a");

            EXPECT_EQ(typing.states(), "C..");
            EXPECT_EQ(typing.cursor(), 1U);
            ASSERT_EQ(typing.events(), 1U);
            EXPECT_EQ(typing.model().log().events()[0].typed.view(), "a");
            EXPECT_EQ(typing.model().log().events()[0].kind, KeystrokeKind::Character);
        }

        TEST(TypingModelTest, AWrongGraphemeIsIncorrectAndStillAdvances) {
            // Not blocking is the default rule: stop_on_error is off
            // (GAMEPLAY section 6), and the variants arrive in TI-034.
            testing::TypingRun typing("abc");
            typing.type("x");

            EXPECT_EQ(typing.states(), "x..");
            EXPECT_EQ(typing.cursor(), 1U);
        }

        TEST(TypingModelTest, BackspaceOverAnErrorReturnsThePositionToPending) {
            testing::TypingRun typing("abc");
            typing.type("x").backspace();

            EXPECT_EQ(typing.states(), "...");
            EXPECT_EQ(typing.cursor(), 0U);
            EXPECT_EQ(typing.events(), 2U) << "the backspace is an event, not the removal of one";
        }

        TEST(TypingModelTest, ACorrectedPositionIsNotACorrectOne) {
            // The distinction the whole accuracy story rests on: this run ends
            // with the right text, and it was still typed wrong once.
            testing::TypingRun typing("abc");
            typing.type("x").backspace().type("a");

            EXPECT_EQ(typing.states(), "c..");
            EXPECT_EQ(typing.cursor(), 1U);
        }

        TEST(TypingModelTest, RetypingAPositionThatWasNeverWrongIsStillCorrect) {
            // Backspacing over correct work — to fix something earlier — must
            // not invent an error that never happened.
            testing::TypingRun typing("abc");
            typing.type("ab").backspace().type("b");

            EXPECT_EQ(typing.states(), "CC.");
        }

        TEST(TypingModelTest, ThePositionRemembersItWasWrongAcrossManyCorrections) {
            testing::TypingRun typing("abc");
            typing.type("x").backspace().type("y").backspace().type("a");

            EXPECT_EQ(typing.states(), "c..");
        }

        TEST(TypingModelTest, BackspaceAtTheStartIsANoOpAndLogsNothing) {
            // Defect C7's boundary. `cursor - 1` at zero is where the legacy
            // engine would have wrapped to SIZE_MAX; here there is nothing to
            // wrap, and nothing is recorded either.
            testing::TypingRun typing("abc");
            typing.backspace(5);

            EXPECT_EQ(typing.cursor(), 0U);
            EXPECT_EQ(typing.states(), "...");
            EXPECT_EQ(typing.events(), 0U);
        }

        TEST(TypingModelTest, TypingPastTheEndChangesNothing) {
            testing::TypingRun typing("ab");
            typing.type("ab");
            ASSERT_TRUE(typing.model().at_end());

            typing.type("cde");

            EXPECT_EQ(typing.cursor(), 2U);
            EXPECT_EQ(typing.states(), "CC");
            EXPECT_EQ(typing.events(), 2U) << "an overrun is not an attempt at a position that exists";
        }

        TEST(TypingModelTest, AnEmptyTextIsFinishedBeforeItStarts) {
            // The legacy code survives this only because should_finish_game()
            // happens to return early. Here it is the asserted behaviour.
            testing::TypingRun typing("");

            EXPECT_TRUE(typing.model().at_end());
            EXPECT_EQ(typing.model().size(), 0U);
            EXPECT_EQ(typing.states(), "");

            typing.type("abc").backspace(3);

            EXPECT_EQ(typing.cursor(), 0U);
            EXPECT_EQ(typing.events(), 0U);
        }

        TEST(TypingModelTest, ASpaceMidWordSkipsToTheNextWordAndMarksTheRestMissed) {
            testing::TypingRun typing("alpha beta");
            typing.type("al ");

            EXPECT_EQ(typing.states(), "CCMMMC....");
            EXPECT_EQ(typing.cursor(), 6U) << "the cursor lands on the first grapheme of the next word";
        }

        TEST(TypingModelTest, ASpaceInTheLastWordSkipsToTheEnd) {
            testing::TypingRun typing("alpha");
            typing.type("al ");

            EXPECT_EQ(typing.states(), "CCMMM");
            EXPECT_TRUE(typing.model().at_end());
            EXPECT_EQ(typing.events(), 3U) << "the space is logged even though it matched no position";
        }

        TEST(TypingModelTest, ASkippedPositionRetypedCorrectlyIsCorrected) {
            // A missed grapheme was still a grapheme you did not get right, so
            // going back for it does not buy a clean Correct.
            testing::TypingRun typing("alpha beta");
            typing.type("al ").backspace(4);

            EXPECT_EQ(typing.states(), "CC........");
            EXPECT_EQ(typing.cursor(), 2U);

            typing.type("pha");

            EXPECT_EQ(typing.states(), "CCccc.....");
        }

        TEST(TypingModelTest, BackspaceAcrossAWordBoundaryRestoresThePreviousWord) {
            testing::TypingRun typing("one two");
            typing.type("one two");
            ASSERT_EQ(typing.states(), "CCCCCCC");

            typing.backspace(5);

            EXPECT_EQ(typing.states(), "CC.....");
            EXPECT_EQ(typing.cursor(), 2U);
        }

        TEST(TypingModelTest, ASpaceCrossesALineBreak) {
            // 1.0 moved to the next line on the space bar, and a document full
            // of newlines is unusable if it demands Enter instead.
            testing::TypingRun typing("one\ntwo");
            typing.type("one two");

            EXPECT_EQ(typing.states(), "CCCCCCC");
            EXPECT_TRUE(typing.model().at_end());
        }

        TEST(TypingModelTest, ASpaceCrossesACarriageReturnLineFeed) {
            // CRLF is one grapheme, so it is one keystroke to cross.
            testing::TypingRun typing("one\r\ntwo");
            const std::size_t size = typing.model().size();
            typing.type("one two");

            EXPECT_EQ(size, 7U);
            EXPECT_EQ(typing.states(), "CCCCCCC");
        }

        TEST(TypingModelTest, AMultiByteGraphemeIsOneKeystroke) {
            testing::TypingRun typing("čšž");
            typing.type("č");

            EXPECT_EQ(typing.states(), "C..");
            EXPECT_EQ(typing.cursor(), 1U);
            EXPECT_EQ(typing.events(), 1U);
        }

        TEST(TypingModelTest, EveryEventCarriesThePositionItWasJudgedAgainst) {
            testing::TypingRun typing("ab cd");
            typing.type("ab").backspace().type("b c");

            const std::span<const Keystroke> events = typing.model().log().events();
            ASSERT_EQ(events.size(), 6U);
            const std::vector<std::uint32_t> targets{events[0].target, events[1].target, events[2].target,
                                                     events[3].target, events[4].target, events[5].target};
            // a, b, the backspace off position 1, the retyped b, the space at
            // 2 and the c at 3.
            EXPECT_EQ(targets, (std::vector<std::uint32_t>{0, 1, 1, 1, 2, 3}));
            EXPECT_EQ(events[2].kind, KeystrokeKind::Backspace);
        }

        TEST(TypingModelTest, TheLogRecordsEveryAttemptInOrder) {
            testing::TypingRun typing("ab");
            typing.type("x").backspace().type("ab");

            const std::span<const Keystroke> events = typing.model().log().events();
            ASSERT_EQ(events.size(), 4U);
            EXPECT_EQ(events[0].typed.view(), "x");
            EXPECT_EQ(events[1].kind, KeystrokeKind::Backspace);
            EXPECT_EQ(events[2].typed.view(), "a");
            EXPECT_EQ(events[3].typed.view(), "b");
            EXPECT_EQ(typing.model().log().duration(), Millis{300});
        }

        // Ported from tests/test_input_line_engine.cpp. The legacy engine
        // counted lines because it stored the input as lines; the rebuild has
        // one flat cursor and computes lines separately (TI-031), so the
        // behaviours port as cursor positions across the same text.
        class LegacyInputLineTest : public ::testing::Test {
        protected:
            LegacyInputLineTest() : typing_{"line1\nline 2\nThird line\nsecond last line\nfinally last line."} {}

            testing::TypingRun typing_;
        };

        TEST_F(LegacyInputLineTest, LineTransitionOnSpace) {
            typing_.type("line1 ");

            EXPECT_EQ(typing_.cursor(), 6U) << "past the newline, at the start of line 2";
            EXPECT_EQ(typing_.states().substr(0, 6), "CCCCCC");
        }

        TEST_F(LegacyInputLineTest, BackspaceAtLineStartReturnsToThePreviousLine) {
            typing_.type("line1 ");
            ASSERT_EQ(typing_.cursor(), 6U);

            typing_.backspace();

            EXPECT_EQ(typing_.cursor(), 5U) << "back onto the line break itself";
            EXPECT_EQ(typing_.states().substr(0, 6), "CCCCC.");
        }

        TEST_F(LegacyInputLineTest, DoNothingWhenNoElements) {
            typing_.backspace();

            EXPECT_EQ(typing_.cursor(), 0U);
            EXPECT_EQ(typing_.events(), 0U);
        }

        TEST_F(LegacyInputLineTest, RemovesTheFirstElement) {
            typing_.type("l");
            ASSERT_EQ(typing_.cursor(), 1U);

            typing_.backspace();

            EXPECT_EQ(typing_.cursor(), 0U);
            EXPECT_EQ(typing_.states().substr(0, 1), ".");
        }

        TEST_F(LegacyInputLineTest, TypingMoreThanTheTextAddsNothing) {
            typing_.type("line1 line 2 Third line second last line finally last line.");
            ASSERT_TRUE(typing_.model().at_end());
            const std::size_t at_end = typing_.events();

            typing_.type("hello");

            EXPECT_EQ(typing_.cursor(), typing_.model().size());
            EXPECT_EQ(typing_.events(), at_end);
        }

        // The two invariants, over sequences nobody would think to write down.
        // Ten thousand of them, each on a fresh model, so a sequence that only
        // misbehaves from a particular starting state gets its chance.
        TEST(TypingModelPropertyTest, TheInvariantsHoldOverTenThousandRandomSequences) {
            const TextBuffer target = testing::text_of("the quick fox\njumps over čšž 漢字 😀 dogs");
            const TextBuffer alphabet = testing::text_of("abc xyz\nčž漢😀");

            std::mt19937 random{20260803};
            std::uniform_int_distribution<std::size_t> pick_grapheme{0, alphabet.size() - 1};
            std::uniform_int_distribution<int> pick_operation{0, 3};
            std::uniform_int_distribution<int> pick_length{0, 60};

            for (int sequence = 0; sequence < 10'000; ++sequence) {
                TypingModel model{target};
                Millis now{0};
                const int steps = pick_length(random);
                for (int step = 0; step < steps; ++step) {
                    now += Millis{7};
                    if (pick_operation(random) == 0) {
                        model.backspace(now);
                    } else {
                        model.type(alphabet.at(GraphemeIndex{pick_grapheme(random)}), now);
                    }

                    ASSERT_LE(model.cursor().value, model.size()) << "sequence " << sequence << " step " << step;
                    ASSERT_EQ(model.states().size(), model.size()) << "sequence " << sequence << " step " << step;
                }

                // And the log is still ordered, with every event pointing at a
                // position that exists.
                const std::span<const Keystroke> events = model.log().events();
                for (std::size_t i = 1; i < events.size(); ++i) {
                    ASSERT_LE(events[i - 1].at, events[i].at) << "sequence " << sequence << " event " << i;
                    ASSERT_LE(events[i].target, model.size()) << "sequence " << sequence << " event " << i;
                }
            }
        }

    }  // namespace
}  // namespace typeit::core
