// The script format, line by line.
//
// Everything here is about refusing what cannot be replayed exactly. A script
// is a fixture: it is checked in, it outlives the person who wrote it, and a
// line that is quietly reinterpreted years later takes a passing test with it.

#include <gtest/gtest.h>
#include <string>
#include <string_view>

#include "typeit/cli/Script.h"
#include "typeit/core/util/Result.h"
#include "typeit/core/util/Units.h"

namespace typeit::cli {
    namespace {

        Script parsed(std::string_view text) {
            const core::Result<Script> script = parse_script(text);
            EXPECT_TRUE(script) << (script ? "" : script.error().context);
            return script.value_or(Script{});
        }

        std::string refusal(std::string_view text) {
            const core::Result<Script> script = parse_script(text);
            EXPECT_FALSE(script) << "expected a refusal";
            if (script) {
                return {};
            }
            EXPECT_EQ(script.error().code, core::ErrorCode::InvalidScript);
            return script.error().context;
        }

        TEST(ScriptParserTest, ReadsTheDocumentedExample) {
            const Script script =
                    parsed("# typeit-script 1\n"
                           "100  type h\n"
                           "250  type e\n"
                           "400  backspace\n"
                           "520  type e\n");

            ASSERT_EQ(script.events.size(), 4U);
            EXPECT_EQ(script.version, 1);
            EXPECT_EQ(script.events.at(0).at, core::Millis{100});
            EXPECT_EQ(script.events.at(0).typed.view(), "h");
            EXPECT_TRUE(script.events.at(2).is_backspace());
            EXPECT_EQ(script.events.at(3).at, core::Millis{520});
        }

        TEST(ScriptParserTest, CommentsAndBlankLinesAreIgnored) {
            const Script script =
                    parsed("# typeit-script 1\n"
                           "\n"
                           "# the typist warms up\n"
                           "100  type h\n"
                           "   \n"
                           "200  type i\n");

            EXPECT_EQ(script.events.size(), 2U);
        }

        TEST(ScriptParserTest, AHeaderAndNothingElseIsARunNobodyTypedIn) {
            const Script script = parsed("# typeit-script 1\n");

            EXPECT_TRUE(script.events.empty());
        }

        TEST(ScriptParserTest, CarriageReturnsSurviveAWindowsEditor) {
            const Script script =
                    parsed("# typeit-script 1\r\n"
                           "100  type h\r\n");

            ASSERT_EQ(script.events.size(), 1U);
            EXPECT_EQ(script.events.front().typed.view(), "h");
        }

        TEST(ScriptParserTest, AnyRunOfSpacesOrTabsSeparatesTheColumns) {
            const Script script =
                    parsed("# typeit-script 1\n"
                           "100\ttype\th\n"
                           "200 type i\n");

            EXPECT_EQ(script.events.size(), 2U);
        }

        TEST(ScriptParserTest, TypeAcceptsAMultiByteGrapheme) {
            const Script script =
                    parsed("# typeit-script 1\n"
                           "100  type č\n"
                           "200  type é\n"
                           "300  type 世\n");

            ASSERT_EQ(script.events.size(), 3U);
            EXPECT_EQ(script.events.at(0).typed.view(), "č");
            EXPECT_EQ(script.events.at(1).typed.view(), "é");
            EXPECT_EQ(script.events.at(2).typed.view(), "世");
        }

        TEST(ScriptParserTest, ASpaceIsSpelledOut) {
            // Trailing whitespace in a fixture is invisible, and the first
            // editor to touch the file would strip it.
            const Script script =
                    parsed("# typeit-script 1\n"
                           "100  type space\n");

            ASSERT_EQ(script.events.size(), 1U);
            EXPECT_EQ(script.events.front().typed.view(), " ");
            EXPECT_FALSE(script.events.front().is_backspace()) << "a space is a keystroke, not an absence";
        }

        TEST(ScriptParserTest, AMissingHeaderIsRefused) {
            EXPECT_EQ(refusal("100  type h\n").substr(0, 7), "line 1:");
            EXPECT_NE(refusal("100  type h\n").find("typeit-script"), std::string::npos);
        }

        TEST(ScriptParserTest, AnEmptyFileIsRefusedForTheSameReason) {
            EXPECT_NE(refusal("").find("no `# typeit-script 1` header"), std::string::npos);
            EXPECT_NE(refusal("\n\n").find("no `# typeit-script 1` header"), std::string::npos);
        }

        TEST(ScriptParserTest, ACommentThatIsNotTheHeaderCannotStandInForIt) {
            EXPECT_NE(refusal("# just a note\n100 type h\n").find("expected the header"), std::string::npos);
        }

        TEST(ScriptParserTest, AnUnsupportedVersionIsRefusedRatherThanGuessedAt) {
            // A format change that silently reinterpreted old scripts would
            // quietly invalidate every fixture in the suite.
            EXPECT_EQ(refusal("# typeit-script 2\n100 type h\n"),
                      "line 1: script version 2 (this binary understands 1)");
            EXPECT_NE(refusal("# typeit-script x\n").find("not a number"), std::string::npos);
        }

        TEST(ScriptParserTest, AnUnknownVerbIsRefusedWithTheKnownOnesNamed) {
            EXPECT_EQ(refusal("# typeit-script 1\n100  tpye h\n"),
                      "line 2: unknown verb \"tpye\" (expected `type` or `backspace`)");
        }

        TEST(ScriptParserTest, NonMonotonicTimestampsAreRefused) {
            // The keystroke log's own precondition. A keyboard cannot travel
            // back in time; a file can, and every window-based metric would be
            // undefined if it did.
            EXPECT_EQ(refusal("# typeit-script 1\n200  type h\n100  type i\n"),
                      "line 3: the timestamp goes backwards: 100 after 200");
        }

        TEST(ScriptParserTest, TwoEventsAtTheSameMillisecondAreAllowed) {
            // Not backwards, and a real keyboard can deliver two events inside
            // one millisecond.
            const Script script =
                    parsed("# typeit-script 1\n"
                           "100  type h\n"
                           "100  type i\n");

            EXPECT_EQ(script.events.size(), 2U);
        }

        TEST(ScriptParserTest, ATimestampThatIsNotANumberIsRefused) {
            EXPECT_EQ(refusal("# typeit-script 1\nsoon  type h\n"),
                      "line 2: expected a timestamp in milliseconds, not \"soon\"");
            EXPECT_NE(refusal("# typeit-script 1\n1.5  type h\n").find("expected a timestamp"), std::string::npos);
        }

        TEST(ScriptParserTest, TypeOfAnythingButOneGraphemeIsRefused) {
            // A line is one keystroke. Reading `type hello` as five would
            // invent timings nobody wrote.
            EXPECT_NE(refusal("# typeit-script 1\n100  type hello\n").find("exactly one grapheme, not 5"),
                      std::string::npos);
            EXPECT_NE(refusal("# typeit-script 1\n100  type\n").find("`type` expects a grapheme"), std::string::npos);
        }

        TEST(ScriptParserTest, BackspaceTakesNoArgument) {
            EXPECT_EQ(refusal("# typeit-script 1\n100  backspace h\n"), "line 2: `backspace` takes no argument");
        }

        TEST(ScriptParserTest, TextThatIsNotTextIsRefused) {
            EXPECT_NE(refusal("# typeit-script 1\n100  type \xff\n").find("not UTF-8"), std::string::npos);
        }

        TEST(ScriptParserTest, TheLineNumberCountsBlanksAndCommentsToo) {
            // A line number that skipped them would send the reader to the
            // wrong line of their own file.
            EXPECT_EQ(refusal("# typeit-script 1\n"
                              "\n"
                              "# a note\n"
                              "100  nonsense\n")
                              .substr(0, 7),
                      "line 4:");
        }

    }  // namespace
}  // namespace typeit::cli
