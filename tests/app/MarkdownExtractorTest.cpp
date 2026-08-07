// Markdown, as typing material (TX-002).
//
// The question every case below asks is the same one: *did somebody write
// this, or did a renderer need it?* Link text was written; the URL beside it
// was not. A heading's words were written; its hashes were not. Code inside a
// fence was written, character for character, which is why the fence is the
// one place nothing is touched at all.

#include <cstddef>
#include <gtest/gtest.h>
#include <string>
#include <string_view>
#include <vector>

#include "typeit/app/ingest/Ingestion.h"
#include "typeit/app/ingest/Markdown.h"
#include "typeit/core/util/Result.h"

namespace typeit::app {
    namespace {

        FetchedContent source(std::string_view markdown, std::string title = {}) {
            FetchedContent content;
            content.bytes.reserve(markdown.size());
            for (const char letter: markdown) {
                content.bytes.push_back(static_cast<std::byte>(static_cast<unsigned char>(letter)));
            }
            content.detected_mime = "text/markdown";
            content.suggested_title = std::move(title);
            return content;
        }

        /// What the extractor makes of some Markdown, with the trailing newline
        /// every line-oriented extractor leaves behind trimmed off, so a case
        /// can be written the way somebody would say it out loud.
        std::string extracted(std::string_view markdown, MarkdownOptions options = {}) {
            const MarkdownExtractor extractor{options};
            const core::Result<ExtractedText> result = extractor.extract(source(markdown));
            if (!result) {
                return "<error: " + result.error().context + ">";
            }
            std::string text = result->text;
            while (text.ends_with('\n')) {
                text.pop_back();
            }
            return text;
        }

        // ---- what the extractor claims -----------------------------------------

        TEST(MarkdownExtractorTest, ItClaimsMarkdownAndNothingElse) {
            // Exactly one type, because TX-001's registry treats two extractors
            // claiming one type as a startup error and plain text used to claim
            // this one.
            const MarkdownExtractor extractor;

            ASSERT_EQ(extractor.mime_types().size(), 1U);
            EXPECT_EQ(extractor.mime_types().front(), "text/markdown");
        }

        // ---- inline markup ------------------------------------------------------

        TEST(MarkdownExtractorTest, EmphasisIsAFontRatherThanText) {
            EXPECT_EQ(extracted("This is *very* **important** and _quite_ __so__."),
                      "This is very important and quite so.");
        }

        TEST(MarkdownExtractorTest, ALinksTextSurvivesAndItsUrlDoesNot) {
            // A URL is slashes and percent-escapes nobody would choose to type;
            // the text beside it is the sentence somebody wrote.
            EXPECT_EQ(extracted("See [the manual](https://example.com/docs?a=1&b=2) first."), "See the manual first.");
        }

        TEST(MarkdownExtractorTest, AnImageGoesEntirelyRatherThanLeavingItsAltText) {
            // Alt text describes a picture to somebody who cannot see it. Typing
            // "photograph of a lighthouse" mid-paragraph is a non sequitur.
            EXPECT_EQ(extracted("Before ![photograph of a lighthouse](lighthouse.png) after."), "Before  after.");
        }

        TEST(MarkdownExtractorTest, InlineCodeKeepsTheCodeAndDropsTheBackticks) {
            EXPECT_EQ(extracted("Call `std::move(x)` on it."), "Call std::move(x) on it.");
        }

        TEST(MarkdownExtractorTest, MarkupInsideInlineCodeIsCodeRatherThanMarkup) {
            // The commonest way a stripper corrupts a technical document: the
            // asterisks in `a * b` are multiplication, and a stripper that does
            // not know it is inside a code span eats them.
            EXPECT_EQ(extracted("The expression `a * b + _c` is arithmetic."),
                      "The expression a * b + _c is arithmetic.");
        }

        TEST(MarkdownExtractorTest, AnEscapedMarkIsTheMarkItself) {
            EXPECT_EQ(extracted(R"(A literal \*asterisk\* and a \[bracket\].)"),
                      "A literal *asterisk* and a [bracket].");
        }

        TEST(MarkdownExtractorTest, AComparisonIsNotATag) {
            // `a < b` is a sentence. Treating every `<` as markup would eat the
            // rest of the line whenever a document does arithmetic.
            EXPECT_EQ(extracted("Where a < b and b > c."), "Where a < b and b > c.");
        }

        // ---- reference links -----------------------------------------------------

        TEST(MarkdownExtractorTest, AReferenceLinkResolvesToItsText) {
            EXPECT_EQ(extracted("Read [the spec][spec] today.\n\n[spec]: https://example.com/spec"),
                      "Read the spec today.");
        }

        TEST(MarkdownExtractorTest, AShortcutReferenceResolvesToItsText) {
            EXPECT_EQ(extracted("Read [the spec] today.\n\n[the spec]: https://example.com/spec"),
                      "Read the spec today.");
        }

        TEST(MarkdownExtractorTest, AnUnresolvedReferenceDegradesToItsText) {
            // A typo in a label should cost the reader a link, not a sentence.
            EXPECT_EQ(extracted("Read [the spec][nosuch] today."), "Read the spec today.");
        }

        TEST(MarkdownExtractorTest, BracketsThatDefineNothingArePunctuation) {
            // `[sic]` and `[1]` are things people write in prose.
            EXPECT_EQ(extracted("He wrote it hisself [sic] in 1904."), "He wrote it hisself [sic] in 1904.");
        }

        TEST(MarkdownExtractorTest, ADefinitionLineIsNotTypingMaterial) {
            EXPECT_EQ(extracted("Text.\n\n[spec]: https://example.com/spec \"The Spec\""), "Text.");
        }

        // ---- block structure ----------------------------------------------------

        TEST(MarkdownExtractorTest, AHeadingKeepsItsWordsAndLosesItsHashes) {
            EXPECT_EQ(extracted("## Getting started ##\n\nText."), "Getting started\n\nText.");
        }

        TEST(MarkdownExtractorTest, AHashWithoutASpaceIsAHashtagRatherThanAHeading) {
            EXPECT_EQ(extracted("#nofilter is not a heading."), "#nofilter is not a heading.");
        }

        TEST(MarkdownExtractorTest, AnUnderlinedLineIsAHeadingToo) {
            EXPECT_EQ(extracted("Chapter One\n===========\n\nText."), "Chapter One\n\nText.");
        }

        TEST(MarkdownExtractorTest, AListLosesItsBulletsAndKeepsItsItems) {
            EXPECT_EQ(extracted("- first\n* second\n+ third\n1. fourth\n2) fifth"),
                      "first\nsecond\nthird\nfourth\nfifth");
        }

        TEST(MarkdownExtractorTest, ABlockQuoteLosesItsMarkersAtEveryDepth) {
            EXPECT_EQ(extracted("> quoted\n>> deeper"), "quoted\ndeeper");
        }

        TEST(MarkdownExtractorTest, ATableKeepsItsCellsAndLosesItsFrame) {
            // The pipes align columns on a screen. Typing them is typing the
            // frame rather than the contents.
            EXPECT_EQ(extracted("| Name | Age |\n|------|-----|\n| Ada  | 36  |"), "Name Age\n\nAda 36");
        }

        TEST(MarkdownExtractorTest, AThematicBreakIsNothingToType) {
            EXPECT_EQ(extracted("Above\n\n---\n\nBelow"), "Above\n\n\nBelow");
        }

        TEST(MarkdownExtractorTest, BlankLinesSurviveBecauseParagraphsAreStructure) {
            EXPECT_EQ(extracted("One.\n\nTwo."), "One.\n\nTwo.");
        }

        // ---- fenced code --------------------------------------------------------

        TEST(MarkdownExtractorTest, AFencedBlockKeepsEveryByteOfItsWhitespace) {
            // A code sample is the one part of a Markdown file that is already
            // what somebody wants to type, and every space in it is
            // load-bearing. Stripping the markup inside it would produce code
            // that does not compile.
            const std::string_view markdown = "```cpp\nif (a * b) {\n    return _x;\n}\n```\n";

            EXPECT_EQ(extracted(markdown), "if (a * b) {\n    return _x;\n}");
        }

        TEST(MarkdownExtractorTest, ATildeFenceIsAFenceToo) { EXPECT_EQ(extracted("~~~\n  kept  \n~~~"), "  kept  "); }

        TEST(MarkdownExtractorTest, AnUnclosedFenceRunsToTheEndRatherThanSwallowingNothing) {
            EXPECT_EQ(extracted("```\ncode\nmore"), "code\nmore");
        }

        // ---- front matter and embedded HTML -------------------------------------

        TEST(MarkdownExtractorTest, YamlFrontMatterIsAMachinesMetadata) {
            // Typing `layout: post` is typing a build system's configuration.
            EXPECT_EQ(extracted("---\ntitle: A post\nlayout: post\n---\n\nThe prose."), "\nThe prose.");
        }

        TEST(MarkdownExtractorTest, TomlFrontMatterGoesTheSameWay) {
            EXPECT_EQ(extracted("+++\ntitle = \"A post\"\n+++\n\nThe prose."), "\nThe prose.");
        }

        TEST(MarkdownExtractorTest, AnUnterminatedFrontMatterFenceIsAThematicBreak) {
            // Otherwise a document opening with a horizontal rule would be
            // swallowed entirely, which is a spectacular way to fail.
            EXPECT_EQ(extracted("---\n\nThe prose."), "\nThe prose.");
        }

        TEST(MarkdownExtractorTest, EmbeddedHtmlIsStripped) {
            EXPECT_EQ(extracted("A <b>bold</b> claim.<!-- and a note -->"), "A  bold  claim.");
        }

        // ---- sections -----------------------------------------------------------

        TEST(MarkdownExtractorTest, EachHeadingStartsASection) {
            const MarkdownExtractor extractor;

            const core::Result<ExtractedText> result = extractor.extract(source("# One\n\nalpha\n\n## Two\n\nbeta\n"));

            ASSERT_TRUE(result);
            ASSERT_EQ(result->sections.size(), 2U);
            EXPECT_EQ(result->sections[0].title, "One");
            EXPECT_EQ(result->sections[1].title, "Two");
            EXPECT_EQ(result->text.substr(result->sections[1].start, result->sections[1].length), "Two\n\nbeta\n");
        }

        TEST(MarkdownExtractorTest, TheSectionsCoverTheWholeTextWithoutGaps) {
            // A bookmark measured in offsets has to land inside a section
            // wherever it lands, including in the prose before the first
            // heading.
            const MarkdownExtractor extractor;

            const core::Result<ExtractedText> result = extractor.extract(source("preamble\n\n# One\n\nalpha\n"));

            ASSERT_TRUE(result);
            ASSERT_EQ(result->sections.size(), 2U);
            EXPECT_EQ(result->sections[0].title, "") << "the preamble belongs to nobody in particular";
            EXPECT_EQ(result->sections[0].start, 0U);
            EXPECT_EQ(result->sections[0].start + result->sections[0].length, result->sections[1].start);
            EXPECT_EQ(result->sections[1].start + result->sections[1].length, result->text.size());
        }

        TEST(MarkdownExtractorTest, ADocumentWithoutHeadingsIsOneUndividedText) {
            const MarkdownExtractor extractor;

            const core::Result<ExtractedText> result = extractor.extract(source("just some prose\n"));

            ASSERT_TRUE(result);
            EXPECT_TRUE(result->sections.empty());
        }

        TEST(MarkdownExtractorTest, TheFirstHeadingNamesTheDocumentBetterThanItsFilenameDoes) {
            const MarkdownExtractor extractor;

            const core::Result<ExtractedText> titled = extractor.extract(source("# Real Title\n\nText.", "untitled-3"));
            const core::Result<ExtractedText> untitled = extractor.extract(source("Text.", "untitled-3"));

            ASSERT_TRUE(titled);
            ASSERT_TRUE(untitled);
            ASSERT_TRUE(titled->title.has_value());
            ASSERT_TRUE(untitled->title.has_value());
            EXPECT_EQ(*titled->title, "Real Title");
            EXPECT_EQ(*untitled->title, "untitled-3") << "and the filename when there is no heading";
        }

        // ---- preserving the markup ----------------------------------------------

        TEST(MarkdownExtractorTest, PreservingMarkupReturnsTheSourceExactly) {
            // For somebody practising Markdown itself. The syntax is a fair
            // share of what a technical writer types all day, and stripping it
            // removes exactly the characters they came for.
            const std::string_view markdown = "# Title\n\n*emphasis* and [a link](https://example.com)\n";

            EXPECT_EQ(extracted(markdown, MarkdownOptions{.preserve_markup = true}),
                      markdown.substr(0, markdown.size() - 1));
        }

        TEST(MarkdownExtractorTest, PreservingMarkupKeepsFrontMatterToo) {
            const std::string_view markdown = "---\ntitle: A post\n---\n\nProse.";

            EXPECT_EQ(extracted(markdown, MarkdownOptions{.preserve_markup = true}), markdown);
        }

        // ---- degenerate input ----------------------------------------------------

        TEST(MarkdownExtractorTest, AnEmptyDocumentIsEmptyRatherThanACrash) {
            const MarkdownExtractor extractor;

            const core::Result<ExtractedText> result = extractor.extract(source(""));

            ASSERT_TRUE(result);
            EXPECT_TRUE(result->text.empty());
            EXPECT_TRUE(result->sections.empty());
        }

        TEST(MarkdownExtractorTest, UnbalancedMarkupDoesNotRunOffTheEnd) {
            // Every one of these is a half-written line somebody saved: a
            // scanner that reads past the end for any of them is undefined
            // rather than merely wrong.
            EXPECT_EQ(extracted("[unclosed"), "[unclosed");
            EXPECT_EQ(extracted("[text]("), "[text](");
            EXPECT_EQ(extracted("`unclosed"), "`unclosed") << "an unpaired backtick is a backtick, as CommonMark says";
            EXPECT_EQ(extracted("<unclosed"), "<unclosed");
            EXPECT_EQ(extracted("![alt"), "![alt");
            EXPECT_EQ(extracted("\\"), "\\");
        }

        TEST(MarkdownExtractorTest, CrlfLineEndingsDoNotLeaveCarriageReturnsBehind) {
            // A `---\r` is not `---`, so a scanner that keeps the carriage
            // return recognises nothing in a file saved on Windows.
            EXPECT_EQ(extracted("# Title\r\n\r\nProse.\r\n"), "Title\n\nProse.");
        }

    }  // namespace
}  // namespace typeit::app
