// The import pipeline (TI-071).
//
// Two of these tests are load-bearing beyond their own assertion. The order
// fixture pins the documented step order by showing a permutation produces
// something else, so reordering the pipeline breaks a test rather than a hash.
// The idempotence matrix runs every combination of toggles, because the whole
// point of normalising is that the same text imports to the same bytes, and a
// step that is not idempotent makes that depend on how many times it ran.

#include <cstddef>
#include <gtest/gtest.h>
#include <string>
#include <string_view>
#include <vector>

#include "typeit/core/text/TextNormalizer.h"
#include "typeit/core/util/Result.h"

namespace typeit::core {
    namespace {

        std::string normalized(std::string_view text, const NormalizeOptions& options = {}) {
            const Result<std::string> result = normalize(text, options);
            EXPECT_TRUE(result) << (result ? "" : result.error().context);
            return result.value_or(std::string{"<error>"});
        }

        // ---- decoding ------------------------------------------------------

        TEST(TextNormalizerTest, InvalidUtf8IsRejectedWithTheByteOffset) {
            const Result<std::string> result = normalize("fine\xE6\xBC");

            ASSERT_FALSE(result);
            EXPECT_EQ(result.error().code, ErrorCode::InvalidUtf8);
            EXPECT_TRUE(result.error().context.starts_with("byte 4:")) << result.error().context;
        }

        TEST(TextNormalizerTest, EmptyTextNormalisesToEmptyText) {
            // Not an error and not an underflow — the successor to C1.
            EXPECT_EQ(normalized(""), "");
        }

        // ---- line endings --------------------------------------------------

        TEST(TextNormalizerTest, EveryLineEndingBecomesLf) {
            EXPECT_EQ(normalized("a\r\nb\rc\nd"), "a\nb\nc\nd") << "CRLF, a lone CR, and one that was already right";
        }

        TEST(TextNormalizerTest, ACarriageReturnAtTheEndIsStillALineEnding) { EXPECT_EQ(normalized("a\r"), "a\n"); }

        TEST(TextNormalizerTest, LineEndingsCanBeLeftAlone) {
            NormalizeOptions options;
            options.line_endings = false;
            EXPECT_EQ(normalized("a\r\nb", options), "a\r\nb");
        }

        // ---- NFC -----------------------------------------------------------

        TEST(TextNormalizerTest, CombiningMarksAreComposed) {
            EXPECT_EQ(to_nfc(U"é"), U"é") << "e + combining acute is é";
            EXPECT_EQ(to_nfc(U"é"), U"é") << "and é is already é";
        }

        TEST(TextNormalizerTest, MarksAreReorderedIntoCanonicalOrder) {
            // Two marks on one base, given in the wrong order. Canonical
            // ordering sorts by combining class, and only then can the acute
            // compose with the base.
            EXPECT_EQ(to_nfc(U"q̣̇"), to_nfc(U"q̣̇"));
            EXPECT_EQ(to_nfc(U"q̣̇"), U"q̣̇") << "dot below (220) sorts before dot above (230)";
        }

        TEST(TextNormalizerTest, ASingletonIsNotRecomposed) {
            // U+212B ANGSTROM SIGN decomposes to Å and must stay Å: the
            // decomposition is a spelling correction, and recomposing it would
            // undo it.
            EXPECT_EQ(to_nfc(U"Å"), U"Å");
        }

        TEST(TextNormalizerTest, AnExcludedPairDoesNotRecompose) {
            // U+0958 is on the composition exclusion list: it decomposes and
            // stays decomposed.
            EXPECT_EQ(to_nfc(U"क़"), U"क़");
        }

        TEST(TextNormalizerTest, HangulComposesArithmetically) {
            EXPECT_EQ(to_nfc(U"가"), U"가") << "leading + vowel";
            EXPECT_EQ(to_nfc(U"각"), U"각") << "and a trailing jamo on top";
            EXPECT_EQ(to_nfc(U"각"), U"각") << "already a syllable";
        }

        TEST(TextNormalizerTest, ABlockedMarkStaysWhereItIs) {
            // The acute cannot reach the `a` past a mark of the same class:
            // composing across it would move the accent to a different letter.
            EXPECT_EQ(to_nfc(U"à́"), U"à́");
        }

        TEST(TextNormalizerTest, AsciiIsAlreadyNormalised) {
            EXPECT_EQ(to_nfc(U"the quick brown fox"), U"the quick brown fox");
        }

        TEST(TextNormalizerTest, NfcCanBeLeftAlone) {
            NormalizeOptions options;
            options.nfc = false;
            EXPECT_EQ(normalized("é", options), "é");
            EXPECT_EQ(normalized("é"), "é");
        }

        // ---- typographic flattening ----------------------------------------

        TEST(TextNormalizerTest, TypographyIsFlattenedToWhatIsOnTheKeyboard) {
            struct Case {
                std::string_view input;
                std::string_view expected;
            };
            const std::vector<Case> cases{
                    {"‘a’", "'a'"}, {"“a”", "\"a\""}, {"„a‟", "\"a\""},     {"a–b", "a-b"},
                    {"a—b", "a-b"}, {"a‒b", "a-b"},   {"wait…", "wait..."}, {"a b", "a b"},
            };
            for (const Case& test: cases) {
                EXPECT_EQ(normalized(test.input), test.expected) << test.input;
            }
        }

        TEST(TextNormalizerTest, FlatteningCanBeLeftAlone) {
            NormalizeOptions options;
            options.flatten_typography = false;
            EXPECT_EQ(normalized("“quoted”", options), "“quoted”");
        }

        // ---- whitespace ----------------------------------------------------

        TEST(TextNormalizerTest, RunsOfWhitespaceCollapseAndTrailingWhitespaceGoes) {
            EXPECT_EQ(normalized("a   b"), "a b");
            EXPECT_EQ(normalized("a \t b"), "a b");
            EXPECT_EQ(normalized("a   \nb"), "a\nb") << "trailing whitespace before a newline";
            EXPECT_EQ(normalized("a   "), "a") << "and at the end of the text";
        }

        TEST(TextNormalizerTest, BlankLinesSurviveCollapsing) {
            // Newlines are whitespace, and collapsing them would run the
            // paragraphs of a novel into one line.
            EXPECT_EQ(normalized("one\n\ntwo"), "one\n\ntwo");
        }

        TEST(TextNormalizerTest, CollapsingCanBeLeftAlone) {
            NormalizeOptions options;
            options.collapse_whitespace = false;
            options.expand_tabs = false;
            EXPECT_EQ(normalized("a   b  ", options), "a   b  ");
        }

        // ---- simplify ------------------------------------------------------

        TEST(TextNormalizerTest, PunctuationAndCaseComeOffOnRequest) {
            NormalizeOptions options;
            options.strip_punctuation = true;
            options.lowercase = true;

            EXPECT_EQ(normalized("The Cat, Sat.", options), "the cat sat");
        }

        TEST(TextNormalizerTest, StrippingPunctuationIsNotAsciiOnly) {
            NormalizeOptions options;
            options.strip_punctuation = true;
            EXPECT_EQ(normalized("¿qué?", options), "qué") << "the inverted question mark too";
        }

        TEST(TextNormalizerTest, LoweringIsNotAsciiOnly) {
            NormalizeOptions options;
            options.lowercase = true;
            EXPECT_EQ(normalized("ÉCOLE", options), "école");
        }

        TEST(TextNormalizerTest, SymbolsAreNotPunctuation) {
            // Stripping `+` and `=` out of a maths text would not simplify it.
            NormalizeOptions options;
            options.strip_punctuation = true;
            EXPECT_EQ(normalized("1 + 2 = 3", options), "1 + 2 = 3");
        }

        // ---- tabs ----------------------------------------------------------

        TEST(TextNormalizerTest, TabsExpandToTheNextTabStop) {
            NormalizeOptions options;
            options.collapse_whitespace = false;
            options.tab_width = 4;

            EXPECT_EQ(normalized("\tx", options), "    x");
            EXPECT_EQ(normalized("ab\tx", options), "ab  x") << "to the stop, not four more spaces";
            EXPECT_EQ(normalized("a\nb\tx", options), "a\nb   x") << "and the column restarts each line";
        }

        TEST(TextNormalizerTest, TheTabWidthIsHonoured) {
            NormalizeOptions options;
            options.collapse_whitespace = false;
            options.tab_width = 2;
            EXPECT_EQ(normalized("\tx", options), "  x");
        }

        TEST(TextNormalizerTest, CollapsingLeavesNothingForTabExpansionToDo) {
            // The documented order puts collapsing first, so with both on a tab
            // is already one space by the time tabs are expanded. Expanding
            // tabs is for the case where collapsing is off, which is what
            // importing code looks like.
            NormalizeOptions options;
            options.tab_width = 8;
            EXPECT_EQ(normalized("a\tb", options), "a b");
        }

        // ---- order ---------------------------------------------------------

        TEST(TextNormalizerTest, TheDocumentedOrderIsNotTheOnlyOrderThatParses) {
            // Collapsing runs before stripping punctuation. Doing it the other
            // way round is a different answer, which is what makes the order a
            // decision rather than an accident — and why it is pinned here
            // rather than in a comment.
            NormalizeOptions collapse_only;
            collapse_only.strip_punctuation = false;

            NormalizeOptions both;
            both.strip_punctuation = true;

            // Documented order: collapse `a  ,  b` to `a , b`, then drop the
            // comma and tidy up after it.
            EXPECT_EQ(normalized("a  ,  b", both), "a b");

            // Permuted by hand: strip first, and the gap the comma left behind
            // never gets collapsed.
            NormalizeOptions strip_without_collapsing;
            strip_without_collapsing.collapse_whitespace = false;
            strip_without_collapsing.strip_punctuation = true;
            strip_without_collapsing.expand_tabs = false;
            const std::string permuted = normalized(normalized("a  ,  b", strip_without_collapsing), collapse_only);
            EXPECT_EQ(permuted, "a b") << "collapsing afterwards recovers, which is why the pipeline does it";

            EXPECT_NE(normalized("a  ,  b", strip_without_collapsing), normalized("a  ,  b", both));
        }

        // ---- idempotence and purity ----------------------------------------

        /// Every combination of the seven toggles, which is what "for all
        /// toggle combinations" in the acceptance criteria means.
        std::vector<NormalizeOptions> every_combination() {
            std::vector<NormalizeOptions> all;
            for (unsigned bits = 0; bits < 128U; ++bits) {
                NormalizeOptions options;
                options.line_endings = (bits & 1U) != 0;
                options.nfc = (bits & 2U) != 0;
                options.flatten_typography = (bits & 4U) != 0;
                options.collapse_whitespace = (bits & 8U) != 0;
                options.strip_punctuation = (bits & 16U) != 0;
                options.lowercase = (bits & 32U) != 0;
                options.expand_tabs = (bits & 64U) != 0;
                all.push_back(options);
            }
            return all;
        }

        /// Everything the pipeline touches, in one string: both line endings,
        /// a decomposed accent, curly quotes, an em dash, an ellipsis, a
        /// non-breaking space, runs of spaces and tabs, trailing whitespace,
        /// capitals, punctuation, a digit and a non-ASCII letter.
        constexpr std::string_view kNasty = "The “quick”  café\r\n\tbrown—fox é, 3 times…  \r  ";

        TEST(TextNormalizerTest, NormalisingTwiceEqualsNormalisingOnce) {
            for (const NormalizeOptions& options: every_combination()) {
                const std::string once = normalized(kNasty, options);
                EXPECT_EQ(normalized(once, options), once) << "toggle combination produced a moving target";
            }
        }

        TEST(TextNormalizerTest, TheSameInputGivesTheSameOutput) {
            // Pure: no clock, no locale, no filesystem, nothing that could make
            // a second call disagree with the first.
            for (const NormalizeOptions& options: every_combination()) {
                EXPECT_EQ(normalized(kNasty, options), normalized(kNasty, options));
            }
        }

        TEST(TextNormalizerTest, EveryStepCanBeTurnedOffAtOnce) {
            NormalizeOptions nothing;
            nothing.line_endings = false;
            nothing.nfc = false;
            nothing.flatten_typography = false;
            nothing.collapse_whitespace = false;
            nothing.strip_punctuation = false;
            nothing.lowercase = false;
            nothing.expand_tabs = false;

            EXPECT_EQ(normalized(kNasty, nothing), kNasty) << "decode and re-encode is a round trip";
        }

    }  // namespace
}  // namespace typeit::core
