// Source code, as typing material (TX-003).
//
// One promise runs through all of this: the bytes come out as they went in. A
// line indented with two tabs stays two tabs and a line indented with seven
// spaces stays seven spaces, because indentation is most of what makes typing
// code different from typing prose, and a typing test that silently rewrites
// its own text is a test of nothing.

#include <cstddef>
#include <gtest/gtest.h>
#include <string>
#include <string_view>
#include <vector>

#include "typeit/app/ingest/Code.h"
#include "typeit/app/ingest/Ingestion.h"
#include "typeit/core/session/TypingModel.h"
#include "typeit/core/session/TypingRules.h"
#include "typeit/core/text/TextBuffer.h"
#include "typeit/core/text/TextNormalizer.h"
#include "typeit/core/util/Result.h"

namespace typeit::app {
    namespace {

        FetchedContent source(std::string_view code, std::string origin = "sample.cpp") {
            FetchedContent content;
            content.bytes.reserve(code.size());
            for (const char letter: code) {
                content.bytes.push_back(static_cast<std::byte>(static_cast<unsigned char>(letter)));
            }
            content.detected_mime = "text/x-code";
            content.origin = std::move(origin);
            return content;
        }

        std::string extracted(std::string_view code, std::string origin = "sample.cpp", CodeOptions options = {}) {
            const CodeExtractor extractor{options};
            const core::Result<ExtractedText> result = extractor.extract(source(code, std::move(origin)));
            return result ? result->text : "<error: " + result.error().context + ">";
        }

        // ---- what it claims -----------------------------------------------------

        TEST(CodeExtractorTest, ItClaimsCodeAndNothingElse) {
            const CodeExtractor extractor;

            ASSERT_EQ(extractor.mime_types().size(), 1U);
            EXPECT_EQ(extractor.mime_types().front(), "text/x-code");
        }

        // ---- indentation, which is the point ------------------------------------

        TEST(CodeExtractorTest, TabIndentationIsByteIdenticalToTheSource) {
            const std::string_view code = "int main() {\n\tif (x) {\n\t\treturn 1;\n\t}\n}\n";

            EXPECT_EQ(extracted(code), code);
        }

        TEST(CodeExtractorTest, SpaceIndentationIsByteIdenticalToTheSource) {
            const std::string_view code = "def main():\n    if x:\n        return 1\n";

            EXPECT_EQ(extracted(code, "sample.py"), code);
        }

        TEST(CodeExtractorTest, AFileThatMixesBothKeepsBothExactly) {
            // Reported as a warning, not repaired. Rewriting somebody's
            // indentation to be consistent would be editing their file, and the
            // file they want to practise is the one they have.
            const std::string_view code = "class A {\n\tint tabbed;\n    int spaced;\n};\n";

            EXPECT_EQ(extracted(code), code);
        }

        TEST(CodeExtractorTest, TheNormalisationItAsksForLeavesTheWhitespaceAlone) {
            // The library's default collapses runs of whitespace and expands
            // tabs. Applied to code it turns every indented line into one
            // leading space, so the extractor overrides it — and this is the
            // assertion that the override is actually asked for, rather than
            // the caller being trusted to have configured the library for code.
            const CodeExtractor extractor;

            const core::Result<ExtractedText> result = extractor.extract(source("int x;\n"));

            ASSERT_TRUE(result);
            ASSERT_TRUE(result->normalization.has_value());
            EXPECT_FALSE(result->normalization->collapse_whitespace);
            EXPECT_FALSE(result->normalization->expand_tabs);
            EXPECT_FALSE(result->normalization->flatten_typography)
                    << "a curly quote in a literal is part of the program";
            EXPECT_TRUE(result->normalization->line_endings) << "CRLF to LF is still wanted";
        }

        TEST(CodeExtractorTest, CrlfBecomesLfWithoutTouchingTheIndentation) {
            // A `\r` is a line ending rather than trailing whitespace, and
            // conflating the two would leave a carriage return in the middle of
            // the text with "keep trailing whitespace" on.
            const std::string_view code = "int main() {\r\n\treturn 0;\r\n}\r\n";

            EXPECT_EQ(extracted(code), "int main() {\n\treturn 0;\n}\n");
        }

        // ---- trailing whitespace -------------------------------------------------

        TEST(CodeExtractorTest, TrailingWhitespaceIsKeptByDefault) {
            // "Exactly as it went in" is the promise, and a trailing space is a
            // real thing an editor will make somebody type.
            EXPECT_EQ(extracted("int x = 1;   \n"), "int x = 1;   \n");
        }

        TEST(CodeExtractorTest, TrailingWhitespaceGoesWhenAsked) {
            // Separable because invisible characters plus `strict_spaces` is a
            // text somebody has to guess at.
            EXPECT_EQ(extracted("int x = 1;   \n", "sample.cpp", CodeOptions{.keep_trailing_whitespace = false}),
                      "int x = 1;\n");
        }

        // ---- comments ------------------------------------------------------------

        TEST(CodeExtractorTest, CommentsAreKeptByDefault) {
            // They are prose written by a programmer and a large fraction of
            // what anybody types in a working day. A file without them is not
            // the file anybody works on.
            const std::string_view code = "// why this exists\nint x = 1;  // and this\n";

            EXPECT_EQ(extracted(code), code);
        }

        TEST(CodeExtractorTest, CommentsGoWhenAsked) {
            const CodeOptions strip{.strip_comments = true};

            EXPECT_EQ(extracted("// why\nint x = 1;  // and\n", "sample.cpp", strip), "\nint x = 1;\n");
        }

        TEST(CodeExtractorTest, AUrlInsideAStringIsNotAComment) {
            // The commonest way a comment stripper corrupts a file: the `//` in
            // `"http://example.com"` starts no comment, and cutting there cuts
            // the line in half.
            const CodeOptions strip{.strip_comments = true};
            const std::string_view code = "const char* u = \"http://example.com\";\n";

            EXPECT_EQ(extracted(code, "sample.cpp", strip), code);
        }

        TEST(CodeExtractorTest, AnEscapedQuoteDoesNotEndTheString) {
            const CodeOptions strip{.strip_comments = true};
            const std::string_view code = "auto s = \"a\\\"//b\";\n";

            EXPECT_EQ(extracted(code, "sample.cpp", strip), code);
        }

        TEST(CodeExtractorTest, ABlockCommentGoesAndTheCodeInFrontOfItStays) {
            const CodeOptions strip{.strip_comments = true};

            EXPECT_EQ(extracted("int x = 1; /* note\nstill note */\nint y = 2;\n", "sample.cpp", strip),
                      "int x = 1;\n\nint y = 2;\n");
        }

        TEST(CodeExtractorTest, PythonCommentsStartWithAHash) {
            const CodeOptions strip{.strip_comments = true};

            EXPECT_EQ(extracted("# why\nx = 1  # and\n", "sample.py", strip), "\nx = 1\n");
        }

        TEST(CodeExtractorTest, AHashInsideACppStringIsNotAComment) {
            // `#` only starts a comment in the languages where it does.
            const CodeOptions strip{.strip_comments = true};
            const std::string_view code = "auto s = \"#not a comment\";\n";

            EXPECT_EQ(extracted(code, "sample.cpp", strip), code);
        }

        // ---- language ------------------------------------------------------------

        TEST(CodeExtractorTest, TheLanguageComesFromTheExtension) {
            EXPECT_EQ(language_of("src/main.cpp"), "cpp");
            EXPECT_EQ(language_of("src/main.RS"), "rust") << "case does not matter";
            EXPECT_EQ(language_of("script.py"), "python");
            EXPECT_EQ(language_of("Makefile"), "") << "nothing this can name";
            EXPECT_EQ(language_of("thing.xyzzy"), "");
        }

        TEST(CodeExtractorTest, AFileInALanguageItCannotNameStillImports) {
            // With its whitespace intact and no sections, which is the right
            // answer rather than a degraded one.
            const CodeExtractor extractor;
            const std::string_view code = "    indented in some language\n";

            const core::Result<ExtractedText> result = extractor.extract(source(code, "thing.xyzzy"));

            ASSERT_TRUE(result);
            EXPECT_EQ(result->text, code);
            EXPECT_TRUE(result->sections.empty());
            EXPECT_FALSE(result->language.has_value());
        }

        // ---- sections ------------------------------------------------------------

        TEST(CodeExtractorTest, TopLevelDefinitionsBecomeSectionsInCpp) {
            const CodeExtractor extractor;
            const std::string_view code =
                    "#include <cstdio>\n\nint one() {\n    return 1;\n}\n\nint two() {\n"
                    "    return 2;\n}\n";

            const core::Result<ExtractedText> result = extractor.extract(source(code));

            ASSERT_TRUE(result);
            ASSERT_EQ(result->sections.size(), 2U);
            EXPECT_EQ(result->sections[0].title, "int one()");
            EXPECT_EQ(result->sections[1].title, "int two()");
            EXPECT_EQ(result->text.substr(result->sections[1].start, result->sections[1].length),
                      "int two() {\n    return 2;\n}\n");
        }

        TEST(CodeExtractorTest, APreprocessorLineNamesNothing) {
            // It is at column zero like a definition and is not one.
            const CodeExtractor extractor;

            const core::Result<ExtractedText> result = extractor.extract(source("#define X 1\n#include <y>\n"));

            ASSERT_TRUE(result);
            EXPECT_TRUE(result->sections.empty());
        }

        TEST(CodeExtractorTest, TopLevelDefinitionsBecomeSectionsInPython) {
            const CodeExtractor extractor;
            const std::string_view code = "import os\n\ndef one():\n    return 1\n\nclass Two:\n    pass\n";

            const core::Result<ExtractedText> result = extractor.extract(source(code, "sample.py"));

            ASSERT_TRUE(result);
            ASSERT_EQ(result->sections.size(), 2U);
            EXPECT_EQ(result->sections[0].title, "def one():");
            EXPECT_EQ(result->sections[1].title, "class Two:");
        }

        TEST(CodeExtractorTest, TopLevelDefinitionsBecomeSectionsInRust) {
            const CodeExtractor extractor;
            const std::string_view code =
                    "use std::io;\n\nfn one() -> i32 {\n    1\n}\n\nfn two() -> i32 {\n    2\n}\n";

            const core::Result<ExtractedText> result = extractor.extract(source(code, "sample.rs"));

            ASSERT_TRUE(result);
            ASSERT_EQ(result->sections.size(), 2U);
            EXPECT_EQ(result->sections[0].title, "fn one() -> i32");
            EXPECT_EQ(result->sections[1].title, "fn two() -> i32");
        }

        TEST(CodeExtractorTest, AnIndentedDefinitionBelongsToTheOneAboveIt) {
            // A method inside a class is part of the class, and a section per
            // method would put a bookmark in the middle of a type.
            const CodeExtractor extractor;
            const std::string_view code = "class A {\n    void method() {\n    }\n};\n";

            const core::Result<ExtractedText> result = extractor.extract(source(code));

            ASSERT_TRUE(result);
            ASSERT_EQ(result->sections.size(), 1U);
            EXPECT_EQ(result->sections[0].title, "class A");
        }

        // ---- diagnostics ---------------------------------------------------------

        TEST(CodeExtractorTest, MixedIndentationIsAWarningRatherThanARefusal) {
            // Tabs and spaces are invisible and identical on screen, so a file
            // that uses both gives the typist no way to know which the next
            // line wants. It still imports: refusing it would refuse a real
            // file somebody wants to type.
            const CodeExtractor extractor;

            const core::Result<ExtractedText> result =
                    extractor.extract(source("class A {\n\tint tabbed;\n    int spaced;\n};\n"));

            ASSERT_TRUE(result);
            ASSERT_EQ(result->warnings.size(), 1U);
            EXPECT_NE(result->warnings.front().find("tabs and spaces"), std::string::npos) << result->warnings.front();
        }

        TEST(CodeExtractorTest, AConsistentFileWarnsAboutNothing) {
            const CodeExtractor extractor;

            const core::Result<ExtractedText> tabs = extractor.extract(source("int f() {\n\treturn 1;\n}\n"));
            const core::Result<ExtractedText> spaces = extractor.extract(source("int f() {\n    return 1;\n}\n"));

            ASSERT_TRUE(tabs);
            ASSERT_TRUE(spaces);
            EXPECT_TRUE(tabs->warnings.empty());
            EXPECT_TRUE(spaces->warnings.empty());
        }

        TEST(CodeExtractorTest, ABlankLineWithAStraySpaceIsNotAnIndentationStyle) {
            const CodeExtractor extractor;

            const core::Result<ExtractedText> result = extractor.extract(source("int f() {\n\tint x;\n \n}\n"));

            ASSERT_TRUE(result);
            EXPECT_TRUE(result->warnings.empty());
        }

        TEST(CodeExtractorTest, AMinifiedLineIsReportedAsPoorTypingMaterial) {
            // One line of forty thousand characters is technically importable
            // and there is no way to type it, so the import says so rather than
            // leaving somebody to find out at the keyboard.
            const CodeExtractor extractor;

            const core::Result<ExtractedText> result =
                    extractor.extract(source(std::string(900, 'a') + "\n", "bundle.js"));

            ASSERT_TRUE(result);
            ASSERT_EQ(result->warnings.size(), 1U);
            EXPECT_NE(result->warnings.front().find("900"), std::string::npos) << result->warnings.front();
        }

        TEST(CodeExtractorTest, TheLongLineThresholdIsConfigurable) {
            const CodeExtractor lenient{CodeOptions{.long_line = 10'000}};

            const core::Result<ExtractedText> result =
                    lenient.extract(source(std::string(900, 'a') + "\n", "bundle.js"));

            ASSERT_TRUE(result);
            EXPECT_TRUE(result->warnings.empty());
        }

        // ---- rejection -----------------------------------------------------------

        TEST(CodeExtractorTest, NonUtf8IsRejectedWithTheByteThatSaidSo) {
            // "Invalid somewhere in a 40 kB file" is not a diagnosis. Checked
            // here rather than left to normalisation because everything this
            // extractor reports is a statement about a text, and saying "line
            // 40 mixes tabs and spaces" about a JPEG is a confident answer to
            // the wrong question.
            const CodeExtractor extractor;
            FetchedContent content = source("int x = 1;\n");
            content.bytes.push_back(std::byte{0xFF});

            const core::Result<ExtractedText> result = extractor.extract(content);

            ASSERT_FALSE(result);
            EXPECT_EQ(result.error().code, core::ErrorCode::InvalidUtf8);
            EXPECT_NE(result.error().context.find("11"), std::string::npos) << result.error().context;
        }

        TEST(CodeExtractorTest, AnEmptyFileIsEmptyRatherThanACrash) {
            const CodeExtractor extractor;

            const core::Result<ExtractedText> result = extractor.extract(source(""));

            ASSERT_TRUE(result);
            EXPECT_TRUE(result->sections.empty());
            EXPECT_TRUE(result->warnings.empty());
        }

        // ---- strict_spaces, which matters far more here than in prose -----------

        /// Types `keys` into a model over `target`, one grapheme at a time.
        ///
        /// The graphemes have to come from a buffer, and a buffer of exactly
        /// the keys pressed is the honest way to say "somebody typed this".
        void type_all(core::TypingModel& model, const core::TextBuffer& keys) {
            core::Millis now{0};
            for (std::size_t at = 0; at < keys.size(); ++at) {
                now += core::Millis{80};
                model.type(keys.at(core::GraphemeIndex{at}), now);
            }
        }

        TEST(CodeExtractorTest, StrictSpacesMakesEveryPreservedIndentCharacterCount) {
            // The acceptance criterion, and the reason the byte-exact promise
            // above is worth keeping. With the indentation preserved, an
            // indented line is four keystrokes before any letter arrives, and
            // `strict_spaces` decides whether skipping them is an error. Code
            // is where the setting earns its place: in prose a missing space is
            // a typo, and in Python it is a different program.
            const core::Result<core::TextBuffer> target = core::TextBuffer::from_utf8("if x:\n    return 1\n");
            const core::Result<core::TextBuffer> keys = core::TextBuffer::from_utf8("if x:\nr");
            ASSERT_TRUE(target);
            ASSERT_TRUE(keys);

            core::TypingModel strict{*target, core::TypingRules{.strict_spaces = true}};
            type_all(strict, *keys);

            // Position 6 is the first of the four indent spaces.
            EXPECT_EQ(strict.states()[6], core::GraphemeState::Incorrect)
                    << "an indent space skipped is an indent space got wrong";
            EXPECT_EQ(strict.cursor(), core::GraphemeIndex{7}) << "and the cursor moved by exactly one";
        }

        TEST(CodeExtractorTest, WithoutStrictSpacesASkippedIndentIsAbsorbedInstead) {
            // The same keystrokes, the other setting: the typist lands on the
            // `r` rather than four characters short of it.
            const core::Result<core::TextBuffer> target = core::TextBuffer::from_utf8("if x:\n    return 1\n");
            const core::Result<core::TextBuffer> keys = core::TextBuffer::from_utf8("if x:\n ");
            ASSERT_TRUE(target);
            ASSERT_TRUE(keys);

            core::TypingModel lenient{*target, core::TypingRules{.strict_spaces = false}};
            core::TypingModel strict{*target, core::TypingRules{.strict_spaces = true}};
            type_all(lenient, *keys);
            type_all(strict, *keys);

            EXPECT_EQ(lenient.states()[6], core::GraphemeState::Correct);
            EXPECT_EQ(strict.states()[6], core::GraphemeState::Correct)
                    << "a space typed where a space is wanted is right under either setting";
        }

    }  // namespace
}  // namespace typeit::app
