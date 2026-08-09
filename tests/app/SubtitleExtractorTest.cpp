// Subtitles, as typing material (TX-004).
//
// Unusually good material: natural conversational prose with realistic
// punctuation, freely available, already short. The one thing wrong with it is
// where the line breaks are — a cue is wrapped to fit a screen, so its breaks
// fall where the width ran out rather than where the sentence did. Typing it as
// written is a test of the subtitler's line-wrapping.

#include <cstddef>
#include <gtest/gtest.h>
#include <string>
#include <string_view>
#include <vector>

#include "typeit/app/ingest/Ingestion.h"
#include "typeit/app/ingest/Subtitles.h"
#include "typeit/core/util/Result.h"

namespace typeit::app {
    namespace {

        FetchedContent source(std::string_view subtitles, std::string title = {}) {
            FetchedContent content;
            content.bytes.reserve(subtitles.size());
            for (const char letter: subtitles) {
                content.bytes.push_back(static_cast<std::byte>(static_cast<unsigned char>(letter)));
            }
            content.detected_mime = "text/x-subrip";
            content.suggested_title = std::move(title);
            return content;
        }

        std::string extracted(std::string_view subtitles) {
            const SubtitleExtractor extractor;
            const core::Result<ExtractedText> result = extractor.extract(source(subtitles));
            return result ? result->text : "<error: " + result.error().context + ">";
        }

        // ---- what it claims -----------------------------------------------------

        TEST(SubtitleExtractorTest, ItClaimsBothDialects) {
            const SubtitleExtractor extractor;

            ASSERT_EQ(extractor.mime_types().size(), 2U);
            EXPECT_EQ(extractor.mime_types()[0], "text/x-subrip");
            EXPECT_EQ(extractor.mime_types()[1], "text/vtt");
        }

        // ---- SubRip --------------------------------------------------------------

        TEST(SubtitleExtractorTest, SubRipExtractsToProseWithNoIndicesOrTimecodes) {
            const std::string_view srt =
                    "1\n"
                    "00:00:01,000 --> 00:00:04,000\n"
                    "Good evening.\n"
                    "\n"
                    "2\n"
                    "00:00:05,000 --> 00:00:08,000\n"
                    "It is a pleasure to be here.\n";

            EXPECT_EQ(extracted(srt), "Good evening.\nIt is a pleasure to be here.\n");
        }

        TEST(SubtitleExtractorTest, ACueWrappedAcrossTwoLinesIsOneSentence) {
            // The line break is where the screen ran out, not where the speaker
            // paused, and typing it as a break is typing the subtitler's
            // formatting.
            const std::string_view srt =
                    "1\n"
                    "00:00:01,000 --> 00:00:04,000\n"
                    "It is a very great pleasure\n"
                    "to be here this evening.\n";

            EXPECT_EQ(extracted(srt), "It is a very great pleasure to be here this evening.\n");
        }

        TEST(SubtitleExtractorTest, ASentenceSpanningTwoCuesIsRejoined) {
            const std::string_view srt =
                    "1\n"
                    "00:00:01,000 --> 00:00:04,000\n"
                    "I was going to say something\n"
                    "\n"
                    "2\n"
                    "00:00:05,000 --> 00:00:08,000\n"
                    "and then I forgot what it was.\n";

            EXPECT_EQ(extracted(srt), "I was going to say something and then I forgot what it was.\n");
        }

        TEST(SubtitleExtractorTest, AQuestionOrExclamationEndsASentenceToo) {
            const std::string_view srt =
                    "1\n00:00:01,000 --> 00:00:02,000\nWhat?\n\n"
                    "2\n00:00:03,000 --> 00:00:04,000\nNothing!\n";

            EXPECT_EQ(extracted(srt), "What?\nNothing!\n");
        }

        TEST(SubtitleExtractorTest, AClosingQuoteAfterTheStopStillEndsIt) {
            const std::string_view srt =
                    "1\n00:00:01,000 --> 00:00:02,000\n\"Go away.\"\n\n"
                    "2\n00:00:03,000 --> 00:00:04,000\nSo he went.\n";

            EXPECT_EQ(extracted(srt), "\"Go away.\"\nSo he went.\n");
        }

        TEST(SubtitleExtractorTest, ATrackTrailingOffMidSentenceIsStillText) {
            EXPECT_EQ(extracted("1\n00:00:01,000 --> 00:00:02,000\nand then he\n"), "and then he\n");
        }

        // ---- WebVTT ---------------------------------------------------------------

        TEST(SubtitleExtractorTest, WebVttExtractsToProseAndItsHeaderGoes) {
            const std::string_view vtt =
                    "WEBVTT\n"
                    "\n"
                    "NOTE this is a comment\n"
                    "\n"
                    "00:00:01.000 --> 00:00:04.000\n"
                    "Good evening.\n";

            EXPECT_EQ(extracted(vtt), "Good evening.\n");
        }

        TEST(SubtitleExtractorTest, PositioningSettingsOnTheTimingLineAreTheRenderersBusiness) {
            const std::string_view vtt =
                    "WEBVTT\n\n"
                    "00:00:01.000 --> 00:00:04.000 align:start position:10%\n"
                    "Good evening.\n";

            EXPECT_EQ(extracted(vtt), "Good evening.\n");
        }

        TEST(SubtitleExtractorTest, ACueIdentifierIsNotDialogue) {
            const std::string_view vtt =
                    "WEBVTT\n\n"
                    "intro\n"
                    "00:00:01.000 --> 00:00:04.000\n"
                    "Good evening.\n";

            EXPECT_EQ(extracted(vtt), "Good evening.\n");
        }

        // ---- tags -----------------------------------------------------------------

        TEST(SubtitleExtractorTest, TagsInsideACueAreStripped) {
            // They are instructions to a renderer. Typing `<i>` is typing
            // markup that was never on screen.
            const std::string_view vtt =
                    "WEBVTT\n\n"
                    "00:00:01.000 --> 00:00:04.000\n"
                    "<i>Good</i> <c.yellow>evening</c>.\n";

            EXPECT_EQ(extracted(vtt), "Good evening.\n");
        }

        TEST(SubtitleExtractorTest, PositioningBracesAreStrippedToo) {
            EXPECT_EQ(extracted("1\n00:00:01,000 --> 00:00:02,000\n{\\an8}Good evening.\n"), "Good evening.\n");
        }

        TEST(SubtitleExtractorTest, AnUnclosedBracketIsPunctuationRatherThanATag) {
            // `a < b` and `:-{` are things people say. Eating the rest of the
            // line for either would lose the dialogue.
            EXPECT_EQ(extracted("1\n00:00:01,000 --> 00:00:02,000\nIs a < b?\n"), "Is a < b?\n");
        }

        // ---- duplicates -------------------------------------------------------------

        TEST(SubtitleExtractorTest, ARepeatedCueIsSaidOnce) {
            // Captions repeat a cue verbatim across a scene change more often
            // than one would think, and a typing test that says the same
            // sentence twice in a row reads as a bug in the test.
            const std::string_view srt =
                    "1\n00:00:01,000 --> 00:00:02,000\nGet out.\n\n"
                    "2\n00:00:03,000 --> 00:00:04,000\nGet out.\n\n"
                    "3\n00:00:05,000 --> 00:00:06,000\nHe left.\n";

            EXPECT_EQ(extracted(srt), "Get out.\nHe left.\n");
        }

        TEST(SubtitleExtractorTest, ARepeatThatIsNotConsecutiveIsSaidAgain) {
            // People do say the same thing twice in a conversation.
            const std::string_view srt =
                    "1\n00:00:01,000 --> 00:00:02,000\nGet out.\n\n"
                    "2\n00:00:03,000 --> 00:00:04,000\nNo.\n\n"
                    "3\n00:00:05,000 --> 00:00:06,000\nGet out.\n";

            EXPECT_EQ(extracted(srt), "Get out.\nNo.\nGet out.\n");
        }

        // ---- malformed input ---------------------------------------------------------

        TEST(SubtitleExtractorTest, AMalformedTimecodeSkipsItsCueAndWarnsRatherThanFailing) {
            // A subtitle file with one corrupt cue in nine hundred is a file
            // worth importing. Refusing the lot over it would be refusing the
            // film.
            const SubtitleExtractor extractor;
            const std::string_view srt =
                    "1\n00:00:01,000 --> 00:00:02,000\nFirst.\n\n"
                    "2\nnonsense --> rubbish\nLost.\n\n"
                    "3\n00:00:05,000 --> 00:00:06,000\nThird.\n";

            const core::Result<ExtractedText> result = extractor.extract(source(srt));

            ASSERT_TRUE(result);
            EXPECT_EQ(result->text, "First.\nThird.\n");
            ASSERT_EQ(result->warnings.size(), 1U);
            EXPECT_NE(result->warnings.front().find("1 cue"), std::string::npos) << result->warnings.front();
        }

        TEST(SubtitleExtractorTest, AFileWithNoCuesSaysSoRatherThanImportingSilence) {
            const SubtitleExtractor extractor;

            const core::Result<ExtractedText> result = extractor.extract(source("just some prose, honestly\n"));

            ASSERT_TRUE(result);
            EXPECT_TRUE(result->text.empty());
            ASSERT_EQ(result->warnings.size(), 1U);
            EXPECT_NE(result->warnings.front().find("no cues"), std::string::npos) << result->warnings.front();
        }

        TEST(SubtitleExtractorTest, AnEmptyFileIsEmptyRatherThanACrash) {
            const SubtitleExtractor extractor;

            const core::Result<ExtractedText> result = extractor.extract(source(""));

            ASSERT_TRUE(result);
            EXPECT_TRUE(result->text.empty());
        }

        TEST(SubtitleExtractorTest, ACueWithNothingButATagContributesNothing) {
            EXPECT_EQ(extracted("1\n00:00:01,000 --> 00:00:02,000\n{\\an8}\n\n"
                                "2\n00:00:03,000 --> 00:00:04,000\nSaid aloud.\n"),
                      "Said aloud.\n");
        }

    }  // namespace
}  // namespace typeit::app
