#include <array>
#include <cstddef>
#include <gtest/gtest.h>
#include <string_view>

#include "typeit/core/text/Grapheme.h"
#include "typeit/core/text/Segmenter.h"
#include "typeit/core/text/Width.h"
#include "typeit/core/text/WidthTables.h"
#include "typeit/core/util/Result.h"

namespace typeit::core {
    namespace {

        /// Columns for a whole string, through the segmenter — which is how every
        /// caller will actually ask.
        std::size_t columns(std::string_view text) {
            const Result<Segmentation> segmented = segment(text);
            EXPECT_TRUE(segmented) << text;
            return segmented ? total_width(segmented->graphemes) : 0;
        }

        TEST(WidthTest, AsciiIsOneColumn) {
            EXPECT_EQ(width_of(U'a'), 1);
            EXPECT_EQ(width_of(U' '), 1);
            EXPECT_EQ(width_of(U'~'), 1);
            EXPECT_EQ(columns("hello"), 5U);
        }

        TEST(WidthTest, EastAsianWideIsTwoColumns) {
            EXPECT_EQ(width_of(U'日'), 2);
            EXPECT_EQ(width_of(U'本'), 2);
            EXPECT_EQ(width_of(U'語'), 2);
            EXPECT_EQ(columns("日本語"), 6U);
        }

        TEST(WidthTest, HangulIsTwoColumns) {
            EXPECT_EQ(width_of(U'한'), 2);
            EXPECT_EQ(columns("한글"), 4U);
        }

        TEST(WidthTest, FullwidthFormsAreTwoColumns) {
            EXPECT_EQ(width_of(U'Ａ'), 2);  // U+FF21 FULLWIDTH LATIN CAPITAL A
            EXPECT_EQ(columns("ＡＢ"), 4U);
        }

        TEST(WidthTest, HalfwidthKatakanaIsOneColumn) {
            EXPECT_EQ(width_of(U'ｱ'), 1);  // U+FF71, Halfwidth: narrow despite being kana
        }

        TEST(WidthTest, CombiningMarksTakeNoColumn) {
            EXPECT_EQ(width_of(U'́'), 0);  // combining acute
            // e + combining acute is one cluster of one column: the mark hangs off the
            // letter rather than sitting beside it.
            EXPECT_EQ(columns("é"), 1U);
            EXPECT_EQ(columns("é"), 1U);  // precomposed, same answer
        }

        TEST(WidthTest, ControlCharactersTakeNoColumn) {
            EXPECT_EQ(width_of(U'\n'), 0);
            EXPECT_EQ(width_of(U'\r'), 0);
            EXPECT_EQ(width_of(U'\0'), 0);
        }

        TEST(WidthTest, EmojiIsTwoColumns) {
            EXPECT_EQ(width_of(U'😀'), 2);
            EXPECT_EQ(columns("😀"), 2U);
        }

        TEST(WidthTest, VariationSelectorsDecidePresentation) {
            // ❤ alone is text presentation and narrow; with VS16 it is the emoji and
            // takes both columns. Getting this wrong is a cursor that drifts by one
            // every time a heart appears.
            EXPECT_EQ(columns("❤"), 1U);
            EXPECT_EQ(columns("❤️"), 2U);
            EXPECT_EQ(columns("\U0001F600︎"), 1U);
        }

        TEST(WidthTest, ZwjSequenceIsOneWideCluster) { EXPECT_EQ(columns("\U0001F468‍\U0001F469"), 2U); }

        TEST(WidthTest, RegionalIndicatorPairIsOneWideCluster) {
            EXPECT_EQ(columns("🇭🇷"), 2U);
            EXPECT_EQ(columns("🇭🇷🇩🇪"), 4U);
        }

        TEST(WidthTest, MixedTextAddsUp) {
            // "a" + "漢" + "😀" + combining-marked "é"
            EXPECT_EQ(columns("a漢😀é"), 1U + 2U + 2U + 1U);
        }

        TEST(WidthTest, TablesRecordTheUnicodeVersionTheyCameFrom) {
            // The generator writes this; a header nobody can date is a header nobody
            // dares regenerate.
            EXPECT_STREQ(tables::kUnicodeVersion, "17.0.0");
        }

        TEST(WidthTest, TablesAreSortedAndDisjoint) {
            // The lookup binary searches, so an unsorted or overlapping table would
            // give wrong answers rather than slow ones.
            const auto check = [](std::span<const tables::CodePointRange> ranges) {
                for (std::size_t i = 0; i < ranges.size(); ++i) {
                    EXPECT_LE(ranges[i].first, ranges[i].last) << "range " << i;
                    if (i > 0) {
                        EXPECT_LT(ranges[i - 1].last, ranges[i].first) << "range " << i;
                    }
                }
            };

            check(tables::kWide);
            check(tables::kZeroWidth);
        }

    }  // namespace
}  // namespace typeit::core
