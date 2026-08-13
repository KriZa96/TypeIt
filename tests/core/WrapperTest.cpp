#include <cstddef>
#include <gtest/gtest.h>
#include <string>
#include <string_view>
#include <vector>

#include "typeit/core/text/TextBuffer.h"
#include "typeit/core/text/Width.h"
#include "typeit/core/text/Wrapper.h"
#include "typeit/core/util/Result.h"
#include "typeit/core/util/Units.h"
#include "typeit/testing/Preconditions.h"

namespace typeit::core {
    namespace {

        /// The legacy wrap threshold, kept as a named constant because every ported
        /// expectation below is measured against it.
        constexpr std::size_t kLegacyColumns = 55;

        TextBuffer build(std::string_view text) {
            Result<TextBuffer> buffer = TextBuffer::from_utf8(text);
            EXPECT_TRUE(buffer) << text;
            return std::move(*buffer);
        }

        std::vector<std::string> lines_of(std::string_view text, std::size_t columns) {
            const TextBuffer buffer = build(text);
            const LineBreaks breaks = wrap(buffer.graphemes(), columns);

            std::vector<std::string> lines;
            lines.reserve(breaks.size());
            for (std::size_t line = 0; line < breaks.size(); ++line) {
                const GraphemeIndex from = breaks.starts[line];
                const GraphemeIndex to =
                        line + 1 < breaks.size() ? breaks.starts[line + 1] : GraphemeIndex{buffer.size()};
                lines.push_back(buffer.to_string(from, to));
            }
            return lines;
        }

        std::size_t line_count(std::string_view text, std::size_t columns = kLegacyColumns) {
            return lines_of(text, columns).size();
        }

        /// Visible width: trailing spaces sit at a break and do not have to fit.
        std::size_t visible_width(std::string_view line) {
            const TextBuffer buffer = build(line);
            std::size_t end = buffer.size();
            while (end > 0 && buffer.at(GraphemeIndex{end - 1}).view() == " ") {
                --end;
            }
            std::size_t columns = 0;
            for (std::size_t i = 0; i < end; ++i) {
                columns += buffer.at(GraphemeIndex{i}).width;
            }
            return columns;
        }

        TEST(WrapperTest, EmptyTextHasNoLines) { EXPECT_EQ(line_count(""), 0U); }

        TEST(WrapperTest, TextShorterThanTheWidthIsOneLine) {
            EXPECT_EQ(lines_of("hello", 20), std::vector<std::string>{"hello"});
        }

        TEST(WrapperTest, AnExactFitIsOneLineWithNoEmptyLineAfterIt) {
            EXPECT_EQ(lines_of("12345", 5), std::vector<std::string>{"12345"});
            EXPECT_EQ(lines_of(std::string(55, 'a'), kLegacyColumns).size(), 1U);
        }

        TEST(WrapperTest, BreaksAtTheLastSpaceThatFits) {
            EXPECT_EQ(lines_of("alpha beta gamma", 12), (std::vector<std::string>{"alpha beta ", "gamma"}));
        }

        TEST(WrapperTest, TrailingSpacesStayOnTheLineTheyEnded) {
            // Absorbed rather than carried: the next line starts with a letter, so the
            // text does not appear to be indented after every wrap.
            const std::vector<std::string> lines = lines_of("alpha   beta", 8);

            ASSERT_EQ(lines.size(), 2U);
            EXPECT_EQ(lines[0], "alpha   ");
            EXPECT_EQ(lines[1], "beta");
        }

        TEST(WrapperTest, AWordLongerThanTheLineIsHardBroken) {
            EXPECT_EQ(lines_of("abcdefghij", 4), (std::vector<std::string>{"abcd", "efgh", "ij"}));
            EXPECT_EQ(lines_of("hi abcdefghij", 4), (std::vector<std::string>{"hi ", "abcd", "efgh", "ij"}));
        }

        TEST(WrapperTest, WideCharactersDoNotOverflowTheBudget) {
            // Three columns cannot hold two wide characters, and a wide character must
            // not be allowed to straddle the boundary.
            const std::vector<std::string> lines = lines_of("日本語", 3);

            ASSERT_EQ(lines.size(), 3U);
            for (const std::string& line: lines) {
                EXPECT_LE(visible_width(line), 3U) << line;
            }
        }

        TEST(WrapperTest, OneColumnGivesOneGraphemePerLine) {
            EXPECT_EQ(lines_of("abc", 1), (std::vector<std::string>{"a", "b", "c"}));
        }

        TEST(WrapperTest, AGraphemeWiderThanTheLineGetsALineOfItsOwn) {
            // It overflows, because there is nowhere else to put it. Documented in the
            // header and asserted here so it is a decision rather than an accident.
            const std::vector<std::string> lines = lines_of("a日b", 1);

            EXPECT_EQ(lines, (std::vector<std::string>{"a", "日", "b"}));
        }

        TEST(WrapperTestDeath, ZeroColumnsIsRejected) {
            const TextBuffer buffer = build("abc");

            TYPEIT_EXPECT_PRECONDITION(static_cast<void>(wrap(buffer.graphemes(), 0)), "at least one column");
        }

        // Ported from tests/test_text.cpp. The rebuild has to produce the same layout
        // the application has always produced, or every user's muscle memory for where
        // the lines fall is wrong.
        TEST(WrapperTest, ReproducesTheLegacyLineCounts) {
            EXPECT_EQ(line_count(""), 0U);
            EXPECT_EQ(line_count("Hello"), 1U);
            EXPECT_EQ(line_count("Hello\nNo\nHello"), 3U);
            EXPECT_EQ(line_count("\n\n"), 2U);
            EXPECT_EQ(line_count("\n\nA"), 3U);
            EXPECT_EQ(line_count("Lorem ipsum dolor sit amet, consectetur adipiscing."), 1U);
            EXPECT_EQ(line_count("Lorem ipsum dolor sit amet, consectetur adipiscing elit yo."), 2U);
            EXPECT_EQ(line_count("Lorem ipsum dolor sit amet, consectetur adipiscing elit. Nulla convallis, urna id "
                                 "fringilla volutpat, sapien justo tincidunt urna."),
                      3U);
            // The threshold itself: 53 a's plus " b" is exactly 55 columns and fits;
            // 54 does not.
            EXPECT_EQ(line_count(std::string(53, 'a') + " b"), 1U);
            EXPECT_EQ(line_count(std::string(54, 'a') + " b"), 2U);
            EXPECT_EQ(line_count(" "), 1U);
            EXPECT_EQ(line_count("a b "), 1U);
            EXPECT_EQ(line_count("\n"), 1U);
        }

        TEST(WrapperTest, DivergesFromTheLegacyCountWhereTheLegacyOverflowed) {
            // The one legacy expectation this does not reproduce, and deliberately.
            //
            // 1.0 breaks at a space only once the line already holds 55 characters, so
            // a line with no space near the limit simply runs past it: this input's
            // third line was 57 columns wide in a 55-column view, and the terminal
            // wrapped it anyway. Four lines is the honest answer; three was a line
            // that did not fit.
            constexpr std::string_view text =
                    "Lorem ipsum dolor sit amet,\nconsectetur adipiscing elit. Nulla convallis,\n urna id fringilla "
                    "volutpat, sapien justo tincidunt urna.";

            EXPECT_EQ(line_count(text), 4U);
            for (const std::string& line: lines_of(text, kLegacyColumns)) {
                EXPECT_LE(visible_width(line), kLegacyColumns) << line;
            }
        }

        // The two properties. A corpus rather than one example, because wrapping is
        // where an off-by-one hides between the cases anybody thinks to write down.
        class WrapperPropertyTest : public ::testing::TestWithParam<std::size_t> {};

        TEST_P(WrapperPropertyTest, LinesConcatenateBackToTheInput) {
            const std::size_t columns = GetParam();
            const std::vector<std::string_view> corpus{
                    "",
                    "a",
                    " ",
                    "hello world",
                    "the quick brown fox jumps over the lazy dog",
                    "čšž ćđ ČŠŽ ĆĐ diacritics everywhere",
                    "日本語のテキストと English mixed together",
                    "emoji 😀 and flags 🇭🇷 and hearts ❤️ in a line",
                    "supercalifragilisticexpialidocious antidisestablishmentarianism",
                    "line one\nline two\n\nline four",
                    "trailing spaces    \nand   internal   runs",
                    "\r\nwindows\r\nline\r\nendings\r\n",
            };

            for (const std::string_view text: corpus) {
                std::string rejoined;
                for (const std::string& line: lines_of(text, columns)) {
                    rejoined += line;
                }
                EXPECT_EQ(rejoined, text) << "columns " << columns;
            }
        }

        TEST_P(WrapperPropertyTest, NoLineExceedsTheColumnBudget) {
            const std::size_t columns = GetParam();
            const std::vector<std::string_view> corpus{
                    "the quick brown fox jumps over the lazy dog",
                    "čšž ćđ ČŠŽ ĆĐ diacritics everywhere",
                    "日本語のテキストと English mixed together",
                    "emoji 😀 and flags 🇭🇷 and hearts ❤️ in a line",
                    "supercalifragilisticexpialidocious antidisestablishmentarianism",
                    "line one\nline two\n\nline four",
            };

            for (const std::string_view text: corpus) {
                for (const std::string& line: lines_of(text, columns)) {
                    // A single grapheme wider than the whole line is the documented
                    // exception; it cannot be split.
                    const TextBuffer buffer = build(line);
                    if (buffer.size() == 1) {
                        continue;
                    }
                    EXPECT_LE(visible_width(line), columns) << "columns " << columns << ", line " << line;
                }
            }
        }

        INSTANTIATE_TEST_SUITE_P(Widths, WrapperPropertyTest,
                                 ::testing::Values(1, 2, 3, 5, 8, 13, 21, 40, 55, 80, 120));

    }  // namespace
}  // namespace typeit::core
