#include <cstddef>
#include <gtest/gtest.h>
#include <string>
#include <string_view>

#include "typeit/core/text/TextBuffer.h"
#include "typeit/core/util/Result.h"
#include "typeit/core/util/Units.h"
#include "typeit/testing/AllocationCounter.h"
#include "typeit/testing/Preconditions.h"

namespace typeit::core {
    namespace {

        TextBuffer build(std::string_view text) {
            Result<TextBuffer> buffer = TextBuffer::from_utf8(text);
            EXPECT_TRUE(buffer) << text;
            return std::move(*buffer);
        }

        std::size_t words_in(std::string_view text) { return build(text).word_count(); }

        TEST(TextBufferTest, RejectsInvalidUtf8NamingTheByte) {
            const Result<TextBuffer> buffer = TextBuffer::from_utf8("fine\xE6\xBC");

            ASSERT_FALSE(buffer);
            EXPECT_EQ(buffer.error().code, ErrorCode::InvalidUtf8);
            EXPECT_TRUE(buffer.error().context.starts_with("byte 4:")) << buffer.error().context;
        }

        TEST(TextBufferTest, EmptyTextIsAValidBuffer) {
            const Result<TextBuffer> buffer = TextBuffer::from_utf8("");

            ASSERT_TRUE(buffer);
            EXPECT_EQ(buffer->size(), 0U);
            EXPECT_TRUE(buffer->empty());
            EXPECT_EQ(buffer->word_count(), 0U);
            EXPECT_EQ(buffer->display_width(), 0U);
            EXPECT_EQ(buffer->to_string(), "");
        }

        TEST(TextBufferTest, IndexesInClustersNotBytes) {
            const TextBuffer buffer = build("ačš😀");

            ASSERT_EQ(buffer.size(), 4U);
            EXPECT_EQ(buffer.at(GraphemeIndex{0}).view(), "a");
            EXPECT_EQ(buffer.at(GraphemeIndex{1}).view(), "č");
            EXPECT_EQ(buffer.at(GraphemeIndex{2}).view(), "š");
            EXPECT_EQ(buffer.at(GraphemeIndex{3}).view(), "😀");
        }

        TEST(TextBufferTestDeath, AtRejectsAnIndexPastTheEnd) {
            const TextBuffer buffer = build("ab");

            TYPEIT_EXPECT_PRECONDITION(static_cast<void>(buffer.at(GraphemeIndex{2})), "out of range");
        }

        TEST(TextBufferTest, AtCheckedReturnsAnErrorInsteadOfDying) {
            const TextBuffer buffer = build("ab");

            EXPECT_TRUE(buffer.at_checked(GraphemeIndex{1}));
            const Result<Grapheme> past_end = buffer.at_checked(GraphemeIndex{2});
            ASSERT_FALSE(past_end);
            EXPECT_EQ(past_end.error().context, "index 2 of 2");
        }

        TEST(TextBufferTest, ToStringRoundTrips) {
            constexpr std::string_view text = "The quick čšž 漢字 😀";
            const TextBuffer buffer = build(text);

            EXPECT_EQ(buffer.to_string(), text);
            EXPECT_EQ(buffer.to_string(GraphemeIndex{0}, GraphemeIndex{3}), "The");
            EXPECT_EQ(buffer.to_string(GraphemeIndex{4}, GraphemeIndex{9}), "quick");
        }

        TEST(TextBufferTest, AnEmptyRangeYieldsAnEmptyString) {
            const TextBuffer buffer = build("abc");

            EXPECT_EQ(buffer.to_string(GraphemeIndex{1}, GraphemeIndex{1}), "");
            EXPECT_EQ(buffer.to_string(GraphemeIndex{3}, GraphemeIndex{3}), "");
        }

        TEST(TextBufferTestDeath, ToStringRejectsAReversedRange) {
            const TextBuffer buffer = build("abc");

            TYPEIT_EXPECT_PRECONDITION(static_cast<void>(buffer.to_string(GraphemeIndex{2}, GraphemeIndex{1})),
                                       "reversed");
        }

        TEST(TextBufferTestDeath, ToStringRejectsARangePastTheEnd) {
            const TextBuffer buffer = build("abc");

            TYPEIT_EXPECT_PRECONDITION(static_cast<void>(buffer.to_string(GraphemeIndex{0}, GraphemeIndex{4})),
                                       "past the text");
        }

        // The eight cases from tests/test_word_count.cpp, with identical expectations.
        // A WPM figure has to mean the same thing before and after the rebuild, and
        // the only way to know is to keep the old suite's answers.
        TEST(TextBufferWordCountTest, MatchesTheLegacyCounts) {
            EXPECT_EQ(words_in(""), 0U);
            EXPECT_EQ(words_in("one"), 1U);
            EXPECT_EQ(words_in(" one "), 1U);
            EXPECT_EQ(words_in("one "), 1U);
            EXPECT_EQ(words_in(" one"), 1U);
            EXPECT_EQ(words_in("one two"), 2U);
            EXPECT_EQ(words_in("one  two"), 2U);
            EXPECT_EQ(words_in("  one     two   "), 2U);
            EXPECT_EQ(words_in("one\ntwo"), 2U);
            EXPECT_EQ(words_in("one two three"), 3U);
            EXPECT_EQ(words_in("   leading spaces"), 2U);
        }

        TEST(TextBufferWordCountTest, TabsAndCarriageReturnsSeparateWords) {
            EXPECT_EQ(words_in("one\ttwo"), 2U);
            EXPECT_EQ(words_in("one\r\ntwo"), 2U);
            EXPECT_EQ(words_in("one\v\ftwo"), 2U);
        }

        TEST(TextBufferWordCountTest, ANonBreakingSpaceDoesNotSeparateWords) {
            // Which is the entire point of a non-breaking space.
            EXPECT_EQ(words_in("one two"), 1U);
            EXPECT_EQ(words_in("one two three"), 2U);
        }

        TEST(TextBufferWordCountTest, TextWithoutSpacesIsOneWord) {
            // CJK does not delimit words with spaces, so this count is meaningless for
            // it and the speed metrics use five-character words instead. Asserted so
            // the limitation is visible rather than surprising.
            EXPECT_EQ(words_in("日本語のテキスト"), 1U);
            EXPECT_EQ(words_in("日本語 のテキスト"), 2U);
        }

        TEST(TextBufferTest, ReportsDisplayWidthInColumns) {
            EXPECT_EQ(build("abc").display_width(), 3U);
            EXPECT_EQ(build("日本語").display_width(), 6U);
            EXPECT_EQ(build("a😀é").display_width(), 1U + 2U + 1U);
        }

        TEST(TextBufferTest, ReportsTruncatedClusters) {
            // Eighteen bytes of family emoji, over what a Grapheme stores.
            const TextBuffer buffer = build("\U0001F468‍\U0001F469‍\U0001F467");

            EXPECT_EQ(buffer.size(), 1U);
            EXPECT_EQ(buffer.truncated(), 1U);
        }

        TEST(TextBufferTest, AllocationCountDoesNotGrowWithTheText) {
            // The buffer is one contiguous allocation for the clusters, so the
            // count is a constant rather than a function of the text length. A
            // per-cluster allocation — the mistake this guards against — would
            // make the two numbers differ by thousands.
            //
            // Not asserted as literally one: MSVC's debug standard library
            // allocates a bookkeeping proxy per container for its iterator
            // checking, so the constant is platform-dependent even though the
            // data is a single block.
            const std::string small = "the quick brown fox";
            std::string large;
            for (int i = 0; i < 500; ++i) {
                large += "the quick brown fox jumps over the lazy dog, čšž 漢字 😀 ";
            }

            const testing::AllocationGuard small_guard;
            const Result<TextBuffer> small_buffer = TextBuffer::from_utf8(small);
            const std::size_t small_allocations = small_guard.count();

            const testing::AllocationGuard large_guard;
            const Result<TextBuffer> large_buffer = TextBuffer::from_utf8(large);
            const std::size_t large_allocations = large_guard.count();

            ASSERT_TRUE(small_buffer);
            ASSERT_TRUE(large_buffer);
            EXPECT_GT(large_buffer->size(), small_buffer->size() * 100);
            EXPECT_EQ(large_allocations, small_allocations);
            // Loose upper bound: enough room for a debug standard library's
            // bookkeeping, tight enough that a second buffer or a temporary
            // copy would break it.
            EXPECT_LE(small_allocations, 8U) << "allocations per buffer";
        }

    }  // namespace
}  // namespace typeit::core
