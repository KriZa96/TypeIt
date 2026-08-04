#include <cstddef>
#include <gtest/gtest.h>
#include <string>
#include <vector>

#include "typeit/core/session/Keystroke.h"
#include "typeit/core/session/KeystrokeLog.h"
#include "typeit/core/session/TypingModel.h"
#include "typeit/core/session/TypingRules.h"
#include "typeit/testing/TypingRun.h"

namespace typeit::core {
    namespace {

        // TECHNICAL section 6's [typing] block, as a test rather than as a
        // second copy of the defaults somewhere in the config layer.
        TEST(TypingRulesTest, TheDefaultsAreTheDocumentedOnes) {
            constexpr TypingRules defaults;

            EXPECT_EQ(defaults.stop_on_error, StopOnError::Off);
            EXPECT_TRUE(defaults.allow_backspace);
            EXPECT_TRUE(defaults.strict_spaces);
            EXPECT_TRUE(defaults.space_advances_word);
            EXPECT_FALSE(defaults.blind_mode);
            EXPECT_EQ(defaults.confidence_mode, ConfidenceMode::Off);
        }

        // stop_on_error = letter

        TEST(StopOnErrorLetterTest, InputIsBlockedUntilTheErrorIsCorrected) {
            testing::TypingRun typing{"abcd", TypingRules{.stop_on_error = StopOnError::Letter}};
            typing.type("x");

            typing.type("cd");

            EXPECT_EQ(typing.states(), "x...");
            EXPECT_EQ(typing.cursor(), 1U);
            EXPECT_EQ(typing.events(), 1U) << "a refused keystroke never reached a position and is not logged";
        }

        TEST(StopOnErrorLetterTest, CorrectingTheErrorUnblocksInput) {
            testing::TypingRun typing{"abcd", TypingRules{.stop_on_error = StopOnError::Letter}};
            typing.type("x").type("zzz").backspace().type("abc");

            EXPECT_EQ(typing.states(), "cCC.");
            EXPECT_EQ(typing.cursor(), 3U);
        }

        TEST(StopOnErrorLetterTest, ASpaceIsBlockedToo) {
            // Skipping the word is a way out of the error, and blocking means
            // blocking.
            testing::TypingRun typing{"abc def", TypingRules{.stop_on_error = StopOnError::Letter}};
            typing.type("x").type(" ");

            EXPECT_EQ(typing.states(), "x......");
            EXPECT_EQ(typing.cursor(), 1U);
        }

        // stop_on_error = word

        TEST(StopOnErrorWordTest, TypingContinuesInsideTheWordThatHasTheError) {
            testing::TypingRun typing{"abc def", TypingRules{.stop_on_error = StopOnError::Word}};
            typing.type("axc");

            EXPECT_EQ(typing.states(), "CxC....");
            EXPECT_EQ(typing.cursor(), 3U);
        }

        TEST(StopOnErrorWordTest, TheWordBoundaryIsWhereItBlocks) {
            testing::TypingRun typing{"abc def", TypingRules{.stop_on_error = StopOnError::Word}};
            typing.type("axc").type(" def");

            EXPECT_EQ(typing.states(), "CxC....") << "nothing past the space was accepted";
            EXPECT_EQ(typing.cursor(), 3U);
            EXPECT_EQ(typing.events(), 3U);
        }

        TEST(StopOnErrorWordTest, FixingTheWordReleasesTheBoundary) {
            testing::TypingRun typing{"abc def", TypingRules{.stop_on_error = StopOnError::Word}};
            typing.type("axc").backspace(2).type("bc def");

            EXPECT_EQ(typing.states(), "CcCCCCC");
            EXPECT_TRUE(typing.model().at_end());
        }

        TEST(StopOnErrorWordTest, AnErrorInAnEarlierWordDoesNotBlockALaterOne) {
            // The rule is about the word being left, not about the run's
            // history: an uncorrected mistake two words back is the typist's
            // choice to live with.
            testing::TypingRun typing{"abc def ghi", TypingRules{.stop_on_error = StopOnError::Word}};
            typing.type("abc dxf ghi");

            EXPECT_EQ(typing.states(), "CCCCCxC....") << "blocked at the space after def, not before it";
            EXPECT_EQ(typing.cursor(), 7U);
        }

        TEST(StopOnErrorWordTest, ACleanWordAfterASeparatorIsNotBlocked) {
            // The backwards scan stops at the separator that started the word.
            // Without that, an error anywhere earlier in the text would block
            // every word boundary after it for the rest of the run.
            testing::TypingRun typing{"abc def ghi", TypingRules{.stop_on_error = StopOnError::Word}};
            typing.type("abc dxf ghi");
            ASSERT_EQ(typing.cursor(), 7U) << "blocked at the space after the bad word";

            typing.backspace(2).type("ef ghi");

            EXPECT_EQ(typing.states(), "CCCCCcCCCCC") << "and released once that word is clean";
        }

        // allow_backspace

        TEST(AllowBackspaceTest, BackspaceIsANoOpAndLogsNothingWhenItIsOff) {
            testing::TypingRun typing{"abc", TypingRules{.allow_backspace = false}};
            typing.type("ax").backspace(3);

            EXPECT_EQ(typing.states(), "Cx.");
            EXPECT_EQ(typing.cursor(), 2U);
            EXPECT_EQ(typing.events(), 2U);
        }

        // strict_spaces

        TEST(StrictSpacesTest, AMissingSpaceIsAnErrorWhenStrict) {
            testing::TypingRun typing{"ab cd", TypingRules{.strict_spaces = true}};
            typing.type("abcd");

            // The c lands on the space and everything after it is off by one:
            // one missing space costs the rest of the word.
            EXPECT_EQ(typing.states(), "CCxx.");
            EXPECT_EQ(typing.cursor(), 4U);
        }

        TEST(StrictSpacesTest, AMissingSpaceIsAbsorbedWhenItIsNot) {
            testing::TypingRun typing{"ab cd", TypingRules{.strict_spaces = false}};
            typing.type("abcd");

            EXPECT_EQ(typing.states(), "CCCCC");
            EXPECT_TRUE(typing.model().at_end());
            EXPECT_EQ(typing.events(), 4U) << "four keys typed, four events, five positions resolved";
        }

        TEST(StrictSpacesTest, ATypedSpaceStillMatchesWhenSpacesAreNotStrict) {
            testing::TypingRun typing{"ab cd", TypingRules{.strict_spaces = false}};
            typing.type("ab cd");

            EXPECT_EQ(typing.states(), "CCCCC");
        }

        TEST(StrictSpacesTest, AbsorptionAtTheEndOfTheTextDropsTheKeystroke) {
            // Nothing follows the separator, so there is no position for the
            // absorbed keystroke to land on.
            testing::TypingRun typing{"ab ", TypingRules{.strict_spaces = false}};
            typing.type("abc");

            EXPECT_EQ(typing.states(), "CCC");
            EXPECT_TRUE(typing.model().at_end());
        }

        // space_advances_word

        TEST(SpaceAdvancesWordTest, ASpaceMidWordJumpsAndMarksTheSkippedGraphemesMissed) {
            testing::TypingRun typing{"alpha beta", TypingRules{.space_advances_word = true}};
            typing.type("al ");

            EXPECT_EQ(typing.states(), "CCMMMC....");
            EXPECT_EQ(typing.cursor(), 6U);
        }

        TEST(SpaceAdvancesWordTest, ASpaceMidWordIsJustAWrongGraphemeWhenItIsOff) {
            testing::TypingRun typing{"alpha beta", TypingRules{.space_advances_word = false}};
            typing.type("al ");

            EXPECT_EQ(typing.states(), "CCx.......");
            EXPECT_EQ(typing.cursor(), 3U);
        }

        // confidence_mode

        TEST(ConfidenceModeTest, OnRefusesToBackspaceOverACompletedWord) {
            testing::TypingRun typing{"one two", TypingRules{.confidence_mode = ConfidenceMode::On}};
            typing.type("one two");

            typing.backspace(5);

            EXPECT_EQ(typing.states(), "CCCC...") << "the current word gives way, the space does not";
            EXPECT_EQ(typing.cursor(), 4U);
        }

        TEST(ConfidenceModeTest, OnStillAllowsCorrectingTheWordInHand) {
            testing::TypingRun typing{"one two", TypingRules{.confidence_mode = ConfidenceMode::On}};
            typing.type("one txo").backspace(2).type("wo");

            EXPECT_EQ(typing.states(), "CCCCCcC");
        }

        TEST(ConfidenceModeTest, MaxRefusesBackspaceEntirely) {
            testing::TypingRun typing{"one two", TypingRules{.confidence_mode = ConfidenceMode::Max}};
            typing.type("onx").backspace(3);

            EXPECT_EQ(typing.states(), "CCx....");
            EXPECT_EQ(typing.cursor(), 3U);
            EXPECT_EQ(typing.events(), 3U);
        }

        // blind_mode

        TEST(BlindModeTest, ChangesNothingAboutWhatTheModelRecords) {
            // It hides the colouring; it does not change what was typed. The
            // model carries the flag for the renderer (Phase 4) and otherwise
            // ignores it.
            testing::TypingRun seeing{"one two", TypingRules{.blind_mode = false}};
            testing::TypingRun blind{"one two", TypingRules{.blind_mode = true}};
            seeing.type("onx t").backspace(2).type("e two");
            blind.type("onx t").backspace(2).type("e two");

            EXPECT_EQ(blind.states(), seeing.states());
            EXPECT_EQ(blind.cursor(), seeing.cursor());
            EXPECT_EQ(blind.events(), seeing.events());
            EXPECT_TRUE(blind.model().rules().blind_mode);
        }

        // The matrix. Every combination of every rule against one fixed input,
        // checked against what each rule promises rather than against 144
        // hand-written strings — a table of expected states would be a copy of
        // the implementation, and would prove only that it had not changed.
        class RuleMatrixTest : public ::testing::TestWithParam<TypingRules> {};

        TEST_P(RuleMatrixTest, EveryCombinationKeepsItsPromises) {
            const TypingRules rules = GetParam();
            constexpr std::string_view text = "one two three";
            // Wrong letter, a space, a correction and an overrun: enough to
            // reach every rule in the set.
            testing::TypingRun typing{text, rules};
            typing.type("onx two").backspace(3).type("e two three more");

            const std::size_t size = typing.model().size();
            EXPECT_EQ(typing.states().size(), size);
            EXPECT_LE(typing.cursor(), size);

            const std::span<const Keystroke> events = typing.model().log().events();
            std::size_t backspaces = 0;
            for (const Keystroke& event: events) {
                EXPECT_LE(event.target, size);
                backspaces += event.kind == KeystrokeKind::Backspace ? 1U : 0U;
            }

            if (!rules.allow_backspace || rules.confidence_mode == ConfidenceMode::Max) {
                EXPECT_EQ(backspaces, 0U) << "backspace is refused, so no backspace can be logged";
            }
            if (rules.stop_on_error == StopOnError::Off && rules.allow_backspace &&
                rules.confidence_mode == ConfidenceMode::Off) {
                EXPECT_GT(typing.cursor(), 0U) << "nothing refuses anything, so the run got somewhere";
            }
            // A state is only reachable when the rule that produces it is on.
            if (!rules.space_advances_word) {
                EXPECT_EQ(typing.states().find('M'), std::string::npos) << "nothing skips, so nothing is missed";
            }
        }

        std::vector<TypingRules> every_combination() {
            std::vector<TypingRules> combinations;
            for (const StopOnError stop: {StopOnError::Off, StopOnError::Letter, StopOnError::Word}) {
                for (const bool backspace: {false, true}) {
                    for (const bool strict: {false, true}) {
                        for (const bool advances: {false, true}) {
                            for (const bool blind: {false, true}) {
                                for (const ConfidenceMode confidence:
                                     {ConfidenceMode::Off, ConfidenceMode::On, ConfidenceMode::Max}) {
                                    combinations.push_back(TypingRules{.stop_on_error = stop,
                                                                       .allow_backspace = backspace,
                                                                       .strict_spaces = strict,
                                                                       .space_advances_word = advances,
                                                                       .blind_mode = blind,
                                                                       .confidence_mode = confidence});
                                }
                            }
                        }
                    }
                }
            }
            return combinations;
        }

        INSTANTIATE_TEST_SUITE_P(AllRules, RuleMatrixTest, ::testing::ValuesIn(every_combination()));

    }  // namespace
}  // namespace typeit::core
