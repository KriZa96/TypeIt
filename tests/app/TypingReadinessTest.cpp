// The difference between a feature and a trap (TX-007).
//
// An extracted chapter is not a typing test. It has running heads, page
// numbers, footnote markers, words broken across line breaks and typography no
// keyboard produces — and an application that imported it unchanged would mark
// every attempt at `—` wrong, forever, with no explanation.
//
// Every step here is a heuristic, so every case says what the heuristic is
// betting on and what it costs when the bet is wrong.

#include <cstddef>
#include <gtest/gtest.h>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "typeit/app/ingest/Readiness.h"
#include "typeit/app/records/TextLibrary.h"
#include "typeit/app/services/TextLibraryService.h"
#include "typeit/core/text/TextNormalizer.h"
#include "typeit/core/util/Result.h"
#include "typeit/core/util/Units.h"
#include "typeit/testing/FakeClock.h"
#include "typeit/testing/Fakes.h"

namespace typeit::app {
    namespace {

        /// One step at a time. Every case turns off everything but the step it
        /// is about, so a failure names the step rather than the pipeline.
        [[nodiscard]] ReadinessOptions only(bool ReadinessOptions::*step) {
            ReadinessOptions options = ReadinessOptions::none();
            options.*step = true;
            return options;
        }

        [[nodiscard]] std::string ready(std::string_view text, const ReadinessOptions& options) {
            return make_typeable(text, options).text;
        }

        // ---- dehyphenation --------------------------------------------------------

        TEST(TypingReadinessTest, AWordBrokenAcrossALineBreakIsPutBackTogether) {
            // The document itself is the dictionary, and it spells the word out
            // in full elsewhere.
            const std::string_view text = "This is an example of prose.\nHere is another exam-\nple of it.\n";

            const std::string out = ready(text, only(&ReadinessOptions::dehyphenate));

            EXPECT_NE(out.find("example of it."), std::string::npos) << out;
            EXPECT_EQ(out.find("exam-"), std::string::npos) << out;
        }

        TEST(TypingReadinessTest, AGenuineHyphenSurvivesTheLineBreak) {
            // `well` and `known` are both words this document uses on their
            // own, so the hyphen between them was the author's.
            const std::string_view text =
                    "It is well understood, and known to all.\nThis is a well-\nknown result in the field.\n";

            const std::string out = ready(text, only(&ReadinessOptions::dehyphenate));

            EXPECT_NE(out.find("well-known result"), std::string::npos) << out;
        }

        TEST(TypingReadinessTest, TheHyphenatedSpellingElsewhereIsAlsoEvidence) {
            const std::string_view text = "A state-of-the-art machine.\nAnother state-of-the-\nart machine.\n";

            const std::string out = ready(text, only(&ReadinessOptions::dehyphenate));

            EXPECT_NE(out.find("state-of-the-art machine."), std::string::npos) << out;
        }

        TEST(TypingReadinessTest, WithNoEvidenceEitherWayTheHyphenGoes) {
            // A hyphen landing exactly at a line ending is far more often a
            // typesetter's than an author's, and that is the whole bet.
            const std::string_view text = "Consider the follow-\ning.\n";

            const std::string out = ready(text, only(&ReadinessOptions::dehyphenate));

            EXPECT_EQ(out, "Consider the following.\n");
        }

        TEST(TypingReadinessTest, AHyphenWithNoWordBeforeItIsPunctuation) {
            const std::string_view text = "A dash on its own -\nand then more prose.\n";

            const std::string out = ready(text, only(&ReadinessOptions::dehyphenate));

            EXPECT_NE(out.find('-'), std::string::npos) << "it was never a broken word: " << out;
        }

        // ---- running heads and page numbers ---------------------------------------

        TEST(TypingReadinessTest, AShortLineRepeatedThroughoutIsARunningHead) {
            const std::string_view text =
                    "MOBY-DICK\nCall me Ishmael.\n\nMOBY-DICK\nSome years ago.\n\nMOBY-DICK\nNever mind how long.\n";

            const std::string out = ready(text, only(&ReadinessOptions::drop_running_heads));

            EXPECT_EQ(out.find("MOBY-DICK"), std::string::npos) << out;
            EXPECT_NE(out.find("Call me Ishmael."), std::string::npos) << out;
        }

        TEST(TypingReadinessTest, AGenuineRepeatedSentenceIsNotARunningHead) {
            // It ends in a full stop, which a header does not, and that is the
            // whole distinction. A chapter title appearing once is kept for the
            // same reason a header appearing three times is not.
            const std::string_view text = "He said no.\nAnd then:\n\nHe said no.\nAgain:\n\nHe said no.\nFinally.\n";

            const std::string out = ready(text, only(&ReadinessOptions::drop_running_heads));

            EXPECT_NE(out.find("He said no."), std::string::npos) << out;
        }

        TEST(TypingReadinessTest, ATitleAppearingOnceIsKept) {
            const std::string_view text = "CHAPTER ONE\nCall me Ishmael.\n";

            const std::string out = ready(text, only(&ReadinessOptions::drop_running_heads));

            EXPECT_NE(out.find("CHAPTER ONE"), std::string::npos) << out;
        }

        TEST(TypingReadinessTest, APageNumberGoesOnSightInAnyOfItsDressings) {
            // It does not have to prove itself by repeating: a line that is
            // nothing but a number is a page number in any book.
            for (const std::string_view dressing: {"14", "- 14 -", "[14]", "Page 14", "  14  "}) {
                const std::string text = "Some prose here.\n" + std::string{dressing} + "\nAnd more prose.\n";

                const std::string out = ready(text, only(&ReadinessOptions::drop_running_heads));

                EXPECT_EQ(out, "Some prose here.\nAnd more prose.\n") << "for " << dressing;
            }
        }

        TEST(TypingReadinessTest, DroppingAPageNumberDoesNotLeaveADoubleParagraphBreak) {
            // It sat alone between two blank lines, so removing only the number
            // would leave a gap of two — once per page, right through the book.
            const std::string_view text = "Some prose here.\n\n14\n\nAnd more prose.\n";

            EXPECT_EQ(ready(text, only(&ReadinessOptions::drop_running_heads)),
                      "Some prose here.\n\nAnd more prose.\n");
        }

        TEST(TypingReadinessTest, ANumberInsideASentenceIsNotAPageNumber) {
            const std::string_view text = "There were 14 of them.\n";

            EXPECT_EQ(ready(text, only(&ReadinessOptions::drop_running_heads)), text);
        }

        // ---- footnote markers -----------------------------------------------------

        TEST(TypingReadinessTest, ABracketedNumberIsAFootnoteMarker) {
            const std::string_view text = "The whale[12] was white[3].\n";

            EXPECT_EQ(ready(text, only(&ReadinessOptions::strip_footnote_markers)), "The whale was white.\n");
        }

        TEST(TypingReadinessTest, ABracketWithWordsInItIsSomethingSomebodyWrote) {
            // `[sic]` and `[a]` are prose. Only digits are a marker.
            const std::string_view text = "He said it [sic] plainly [a].\n";

            EXPECT_EQ(ready(text, only(&ReadinessOptions::strip_footnote_markers)), text);
        }

        TEST(TypingReadinessTest, SuperscriptDigitsAreMarkersToo) {
            // Which is how a typeset marker arrives when the EPUB kept its
            // styling.
            const std::string_view text = "The whale¹ was white².\n";

            EXPECT_EQ(ready(text, only(&ReadinessOptions::strip_footnote_markers)), "The whale was white.\n");
        }

        TEST(TypingReadinessTest, AnUnclosedBracketIsScannedWithoutRunningOffTheEnd) {
            for (const std::string_view awkward: {"[12", "[", "[]\n", "a[1", "["}) {
                EXPECT_NO_FATAL_FAILURE(
                        static_cast<void>(ready(awkward, only(&ReadinessOptions::strip_footnote_markers))))
                        << "for " << awkward;
            }
        }

        // ---- table of contents ----------------------------------------------------

        TEST(TypingReadinessTest, ALeadingBlockOfDottedLeadersIsATableOfContents) {
            const std::string_view text =
                    "CONTENTS\n\nChapter One ............ 1\nChapter Two ............ 24\n\nCall me Ishmael.\n";

            const std::string out = ready(text, only(&ReadinessOptions::drop_table_of_contents));

            EXPECT_EQ(out.find("Chapter One"), std::string::npos) << out;
            EXPECT_NE(out.find("Call me Ishmael."), std::string::npos) << out;
        }

        TEST(TypingReadinessTest, ADottedLeaderDeepInTheBookIsATableAndIsLeftAlone) {
            std::string text;
            for (int line = 0; line < 60; ++line) {
                text += "A line of ordinary prose that goes on for a while.\n";
            }
            text += "Rainfall ............ 42\n";

            const std::string out = ready(text, only(&ReadinessOptions::drop_table_of_contents));

            EXPECT_NE(out.find("Rainfall"), std::string::npos) << "past the window, so it is a table";
        }

        TEST(TypingReadinessTest, ASentenceTrailingOffIsNotADottedLeader) {
            const std::string_view text = "And then he stopped... 1984 was a long time ago.\n";

            EXPECT_EQ(ready(text, only(&ReadinessOptions::drop_table_of_contents)), text);
        }

        // ---- paragraph rejoin -----------------------------------------------------

        TEST(TypingReadinessTest, AHardWrappedParagraphIsReflowedIntoOneLine) {
            // The wrapping is TypeIt's job; doing it twice gives a ragged
            // column half the width of the terminal.
            const std::string_view text =
                    "Call me Ishmael. Some years ago, never mind how long precisely,\n"
                    "having little or no money in my purse, and nothing particular\n"
                    "to interest me on shore, I thought I would sail about.\n";

            const std::string out = ready(text, only(&ReadinessOptions::rejoin_paragraphs));

            EXPECT_EQ(std::ranges::count(out, '\n'), 1) << out;
            EXPECT_NE(out.find("precisely, having"), std::string::npos) << out;
        }

        TEST(TypingReadinessTest, ADeliberateLineBreakInPoetryIsPreserved) {
            // The heuristic and its limit, in one case: these lines are short
            // because they were meant to be, and nothing else distinguishes
            // them from a wrap. A long-lined poem would be reflowed, and that
            // is the documented cost.
            const std::string_view text =
                    "Tyger Tyger, burning bright,\nIn the forests of the night;\nWhat immortal hand or eye,\n";

            EXPECT_EQ(ready(text, only(&ReadinessOptions::rejoin_paragraphs)), text);
        }

        TEST(TypingReadinessTest, ABlankLineStillSeparatesParagraphs) {
            const std::string_view text =
                    "Call me Ishmael. Some years ago, never mind how long precisely,\n"
                    "having little or no money in my purse, I thought I would sail.\n"
                    "\n"
                    "There now is your insular city of the Manhattoes, belted round\n"
                    "by wharves as Indian isles by coral reefs, commerce surrounds it.\n";

            const std::string out = ready(text, only(&ReadinessOptions::rejoin_paragraphs));

            EXPECT_EQ(std::ranges::count(out, '\n'), 3) << "two paragraphs and the blank between them: " << out;
        }

        TEST(TypingReadinessTest, AListItemStartsItsOwnLine) {
            const std::string_view text =
                    "- The first item, which is long enough to look like a wrapped line.\n"
                    "- The second item, which is also long enough to look like one.\n";

            const std::string out = ready(text, only(&ReadinessOptions::rejoin_paragraphs));

            EXPECT_EQ(std::ranges::count(out, '\n'), 2) << out;
        }

        // ---- Gutenberg boilerplate -------------------------------------------------

        TEST(TypingReadinessTest, GutenbergsHeaderAndFooterAreDetectedAndRemoved) {
            const std::string_view text =
                    "The Project Gutenberg eBook of Moby-Dick\n"
                    "This ebook is for the use of anyone anywhere at no cost.\n"
                    "*** START OF THE PROJECT GUTENBERG EBOOK MOBY-DICK ***\n"
                    "Call me Ishmael.\n"
                    "*** END OF THE PROJECT GUTENBERG EBOOK MOBY-DICK ***\n"
                    "Updated editions will replace the previous one.\n";

            const ReadinessResult result = make_typeable(text, only(&ReadinessOptions::strip_boilerplate));

            EXPECT_EQ(result.text, "Call me Ishmael.\n");
            EXPECT_TRUE(result.report.boilerplate_removed);
        }

        TEST(TypingReadinessTest, ASentenceMentioningGutenbergIsNotAMarker) {
            // It is recognised by the stars as well as the name, because
            // somebody may well want to type a sentence about the archive.
            const std::string_view text = "I downloaded it from Project Gutenberg last night.\n";

            const ReadinessResult result = make_typeable(text, only(&ReadinessOptions::strip_boilerplate));

            EXPECT_EQ(result.text, text);
            EXPECT_FALSE(result.report.boilerplate_removed);
        }

        TEST(TypingReadinessTest, AStartMarkerWithNoEndKeepsEverythingAfterIt) {
            // A truncated download is still a book worth typing.
            const std::string_view text = "Header.\n*** START OF THE PROJECT GUTENBERG EBOOK X ***\nCall me Ishmael.\n";

            EXPECT_EQ(ready(text, only(&ReadinessOptions::strip_boilerplate)), "Call me Ishmael.\n");
        }

        // ---- what a keyboard cannot reach -------------------------------------------

        TEST(TypingReadinessTest, TheReportCountsEveryCharacterAKeyboardCannotReach) {
            const ReadinessResult result = make_typeable("He said — “yes” — and then… nothing.\n");

            std::size_t dashes = 0;
            std::size_t quotes = 0;
            for (const UnreachableGrapheme& grapheme: result.report.unreachable) {
                if (grapheme.text == "—") {
                    dashes = grapheme.count;
                }
                if (grapheme.text == "“") {
                    quotes = grapheme.count;
                }
            }
            EXPECT_EQ(dashes, 2U);
            EXPECT_EQ(quotes, 1U);
        }

        TEST(TypingReadinessTest, TypographyIsMarkedAsSomethingNormalisationWillRescue) {
            // The distinction the report exists for. An em dash becomes a
            // hyphen and stops being a problem; `č` does not, and somebody
            // deserves to know which of the two they are looking at before they
            // agree to type it.
            const ReadinessResult result = make_typeable("An em dash — and a č.\n");

            ASSERT_FALSE(result.report.unreachable.empty());
            for (const UnreachableGrapheme& grapheme: result.report.unreachable) {
                if (grapheme.text == "—") {
                    EXPECT_TRUE(grapheme.flattened);
                }
                if (grapheme.text == "č") {
                    EXPECT_FALSE(grapheme.flattened);
                }
            }
            EXPECT_EQ(result.report.unreachable_after_normalisation(), 1U) << "the č, and not the dash";
        }

        TEST(TypingReadinessTest, WithFlatteningOffEvenTheEmDashIsStillAProblem) {
            core::NormalizeOptions plain;
            plain.flatten_typography = false;

            const ReadinessResult result = make_typeable("An em dash —.\n", {}, plain);

            EXPECT_EQ(result.report.unreachable_after_normalisation(), 1U)
                    << "the report answers for the settings actually in force";
        }

        TEST(TypingReadinessTest, ANonLatinScriptIsCountedByGraphemeRatherThanByByte) {
            const ReadinessResult result = make_typeable("漢字 and 😀\n");

            std::size_t total = 0;
            for (const UnreachableGrapheme& grapheme: result.report.unreachable) {
                total += grapheme.count;
            }
            EXPECT_EQ(total, 3U) << "two ideographs and one emoji, not nine bytes";
        }

        TEST(TypingReadinessTest, PlainAsciiReportsNothingAtAll) {
            const ReadinessResult result = make_typeable("Nothing but ASCII here.\n");

            EXPECT_TRUE(result.report.unreachable.empty());
            EXPECT_TRUE(result.report.empty());
            EXPECT_TRUE(result.report.lines().empty());
        }

        TEST(TypingReadinessTest, InvalidUtf8IsNotThisPassesErrorToReport) {
            // Normalisation validates the UTF-8 and names the byte. A second
            // opinion here would be one with a worse message.
            const ReadinessResult result = make_typeable("bad \xFF byte\n");

            EXPECT_TRUE(result.report.unreachable.empty());
        }

        // ---- the report itself -----------------------------------------------------

        TEST(TypingReadinessTest, TheReportSaysWhatHappenedInCountsRatherThanInAdjectives) {
            const std::string_view text =
                    "MOBY-DICK\nCall me Ishmael.\n\nMOBY-DICK\nSome years[1] ago.\n\nMOBY-DICK\nNever mind.\n";

            const ReadinessResult result = make_typeable(text);

            EXPECT_EQ(result.report.running_heads_dropped, 3U);
            EXPECT_EQ(result.report.footnote_markers_stripped, 1U);
            EXPECT_FALSE(result.report.lines().empty());
        }

        // ---- idempotence and the toggle matrix ---------------------------------------

        /// A document with something for every step to find.
        [[nodiscard]] std::string a_messy_document() {
            return "CONTENTS\n\nChapter One ............ 1\n\n"
                   "*** START OF THE PROJECT GUTENBERG EBOOK X ***\n"
                   "MOBY-DICK\n"
                   "Call me Ishmael. Some years ago, never mind how long precisely,\n"
                   "having little or no money in my purse[12], I thought I would sail\n"
                   "about a little and see the watery part of the world — the exam-\n"
                   "ple of it all.\n"
                   "14\n"
                   "MOBY-DICK\n"
                   "There now is your insular city of the Manhattoes, belted round\n"
                   "by wharves as Indian isles by coral reefs.\n"
                   "MOBY-DICK\n"
                   "*** END OF THE PROJECT GUTENBERG EBOOK X ***\n"
                   "Updated editions will replace the previous one.\n";
        }

        TEST(TypingReadinessTest, RunningThePassTwiceIsRunningItOnce) {
            const std::string once = make_typeable(a_messy_document()).text;

            const std::string twice = make_typeable(once).text;

            EXPECT_EQ(twice, once);
        }

        TEST(TypingReadinessTest, IdempotenceHoldsAcrossEveryCombinationOfTheToggles) {
            // Sixty-four combinations, because "runs twice" is what re-importing
            // a text after changing a setting does, and a step that is stable
            // on its own can still be unstable after another one has moved the
            // lines about.
            constexpr int kSteps = 6;
            const std::string source = a_messy_document();
            for (int mask = 0; mask < (1 << kSteps); ++mask) {
                ReadinessOptions options;
                options.dehyphenate = (mask & 1) != 0;
                options.drop_running_heads = (mask & 2) != 0;
                options.strip_footnote_markers = (mask & 4) != 0;
                options.drop_table_of_contents = (mask & 8) != 0;
                options.rejoin_paragraphs = (mask & 16) != 0;
                options.strip_boilerplate = (mask & 32) != 0;

                const std::string once = make_typeable(source, options).text;
                const std::string twice = make_typeable(once, options).text;

                EXPECT_EQ(twice, once) << "toggle mask " << mask;
            }
        }

        TEST(TypingReadinessTest, EveryStepOffLeavesTheTextExactlyAsItArrived) {
            const std::string source = a_messy_document();

            const ReadinessResult result = make_typeable(source, ReadinessOptions::none());

            EXPECT_EQ(result.text, source);
            EXPECT_EQ(result.report.running_heads_dropped, 0U);
            EXPECT_EQ(result.report.dehyphenated, 0U);
            EXPECT_FALSE(result.report.boilerplate_removed);
        }

        TEST(TypingReadinessTest, AnEmptyTextSurvivesEveryStep) {
            const ReadinessResult result = make_typeable("");

            EXPECT_TRUE(result.text.empty());
            EXPECT_TRUE(result.report.empty());
        }

        // ---- the line map ------------------------------------------------------------

        TEST(TypingReadinessTest, TheLineMapFollowsALineThroughTheDeletions) {
            // Sections are the reason this exists: without it, dropping one
            // running head moves every chapter marker in the book up by a line.
            const std::string_view text = "14\nCHAPTER ONE\n15\nCall me Ishmael.\n";

            const ReadinessResult result = make_typeable(text, only(&ReadinessOptions::drop_running_heads));

            ASSERT_EQ(result.source_lines.size(), 2U);
            EXPECT_EQ(result.line_from_source(1), 0U) << "the chapter heading, one line up";
            EXPECT_EQ(result.line_from_source(3), 1U) << "and the prose, two";
        }

        TEST(TypingReadinessTest, ALineMergedIntoAParagraphMapsToThatParagraph) {
            const std::string_view text =
                    "Call me Ishmael. Some years ago, never mind how long precisely,\n"
                    "having little or no money in my purse, I thought I would sail.\n";

            const ReadinessResult result = make_typeable(text, only(&ReadinessOptions::rejoin_paragraphs));

            ASSERT_EQ(result.source_lines.size(), 1U);
            EXPECT_EQ(result.line_from_source(1), 0U) << "it is inside the paragraph that starts at line zero";
        }

        // ---- through the service -------------------------------------------------------

        class ReadinessImportTest : public ::testing::Test {
        protected:
            testing::FakeTextLibraryRepository library;
            testing::FakeFileSystem files;
            testing::FakeClock clock{core::Millis{1'700'000'000'000}};
            TextLibraryService service{library, files, clock};
        };

        TEST_F(ReadinessImportTest, TheReportArrivesBeforeAnythingIsStored) {
            // The acceptance criterion: somebody sees what this text will cost
            // them *before* it is in their library, and can simply not call
            // import.
            files.add_file("/book.txt", "He said — “yes” — and then… nothing.\n");

            const core::Result<ReadinessReport> report = service.inspect_file("/book.txt");

            ASSERT_TRUE(report) << report.error().context;
            EXPECT_FALSE(report->unreachable.empty());
            EXPECT_EQ(library.texts.size(), 0U) << "inspecting is not importing";
        }

        TEST_F(ReadinessImportTest, InspectingReportsTheSameThingImportingDoes) {
            files.add_file("/book.txt", "MOBY-DICK\nA line.\n\nMOBY-DICK\nAnother.\n\nMOBY-DICK\nA third one.\n");

            const core::Result<ReadinessReport> inspected = service.inspect_file("/book.txt");
            const core::Result<ImportOutcome> imported = service.import_file("/book.txt");

            ASSERT_TRUE(inspected) << inspected.error().context;
            ASSERT_TRUE(imported) << imported.error().context;
            EXPECT_EQ(imported->readiness.running_heads_dropped, inspected->running_heads_dropped);
            EXPECT_EQ(imported->readiness.running_heads_dropped, 3U);
        }

        TEST_F(ReadinessImportTest, InspectingAFileThatIsNotThereFailsTheSameWayImportingDoes) {
            const core::Result<ReadinessReport> report = service.inspect_file("/nowhere.txt");

            ASSERT_FALSE(report);
            EXPECT_EQ(report.error().code, core::ErrorCode::FileNotFound);
        }

        TEST_F(ReadinessImportTest, AnImportedBookArrivesWithoutItsRunningHeads) {
            files.add_file("/book.txt",
                           "MOBY-DICK\nCall me Ishmael.\n\nMOBY-DICK\nSome years ago.\n\nMOBY-DICK\nNo.\n");

            const core::Result<ImportOutcome> outcome = service.import_file("/book.txt");

            ASSERT_TRUE(outcome) << outcome.error().context;
            const core::Result<std::optional<TextItem>> stored = library.get(outcome->id);
            ASSERT_TRUE(stored && stored->has_value());
            EXPECT_EQ((*stored)->content.find("MOBY-DICK"), std::string::npos) << (*stored)->content;
            ASSERT_TRUE((*stored)->content_raw.has_value()) << "and the original is kept, untouched";
            EXPECT_NE((*stored)->content_raw->find("MOBY-DICK"), std::string::npos);
        }

        TEST_F(ReadinessImportTest, ASourceFileIsLeftEntirelyAlone) {
            // Every step would be wrong about code: dehyphenation joins
            // `foo-\nbar`, paragraph rejoin puts a function on one line, and
            // `[1]` is a subscript rather than a footnote.
            files.add_file("/main.py", "values = data[1]\nresult = compute(values)\nprint(result)\n");

            const core::Result<ImportOutcome> outcome = service.import_file("/main.py");

            ASSERT_TRUE(outcome) << outcome.error().context;
            const core::Result<std::optional<TextItem>> stored = library.get(outcome->id);
            ASSERT_TRUE(stored && stored->has_value());
            EXPECT_NE((*stored)->content.find("data[1]"), std::string::npos) << (*stored)->content;
            EXPECT_NE((*stored)->content.find("compute(values)\nprint"), std::string::npos)
                    << "and nothing was reflowed: " << (*stored)->content;
        }

        TEST_F(ReadinessImportTest, TheSectionsStillLandWhereTheyShouldAfterLinesAreDropped) {
            // The interaction the line map exists for: the readiness pass
            // deletes lines between the extractor finding a chapter and the
            // section being measured.
            files.add_file("/doc.md",
                           "# One\n\n14\n\nalpha, and a good deal more prose to make this a paragraph.\n\n"
                           "## Two\n\n15\n\nbeta, with an equally respectable quantity of words in it.\n");

            const core::Result<ImportOutcome> outcome = service.import_file("/doc.md");

            ASSERT_TRUE(outcome) << outcome.error().context;
            const core::Result<std::optional<TextItem>> stored = library.get(outcome->id);
            ASSERT_TRUE(stored && stored->has_value());
            ASSERT_EQ((*stored)->sections.size(), 2U);
            const core::Result<core::TextBuffer> buffer = core::TextBuffer::from_utf8((*stored)->content);
            ASSERT_TRUE(buffer);
            EXPECT_EQ(buffer->to_string((*stored)->sections[1].start, (*stored)->sections[1].end),
                      "Two\n\nbeta, with an equally respectable quantity of words in it.\n")
                    << "the whole text was: " << (*stored)->content;
        }

    }  // namespace
}  // namespace typeit::app
