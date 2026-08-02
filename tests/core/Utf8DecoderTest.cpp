#include <cctype>
#include <cstddef>
#include <gtest/gtest.h>
#include <string>
#include <string_view>
#include <vector>

#include "typeit/core/text/Utf8.h"
#include "typeit/core/util/Result.h"
#include "typeit/testing/AllocationCounter.h"

namespace typeit::core {
    namespace {

        /// gtest rejects a parameter name containing anything but letters, digits and
        /// underscores, so the readable names in the tables are converted rather than
        /// written twice.
        std::string sanitised(std::string_view name) {
            std::string result{name};
            for (char& character: result) {
                if (std::isalnum(static_cast<unsigned char>(character)) == 0) {
                    character = '_';
                }
            }
            return result;
        }

        struct ValidCase {
            std::string_view name;
            std::string_view input;
            char32_t code_point;
            std::uint8_t length;
        };

        class Utf8ValidTest : public ::testing::TestWithParam<ValidCase> {};

        TEST_P(Utf8ValidTest, DecodesToTheRightCodePoint) {
            const ValidCase& test = GetParam();

            const Result<DecodedCodePoint> decoded = decode_one(test.input, 0);

            ASSERT_TRUE(decoded) << to_string(decoded.error());
            EXPECT_EQ(decoded->code_point, test.code_point);
            EXPECT_EQ(decoded->length, test.length);
        }

        INSTANTIATE_TEST_SUITE_P(Table, Utf8ValidTest,
                                 ::testing::Values(ValidCase{"ascii NUL", std::string_view{"\0", 1}, 0x0000, 1},
                                                   ValidCase{"ascii letter", "a", 0x0061, 1},
                                                   ValidCase{"ascii max", "\x7F", 0x007F, 1},
                                                   ValidCase{"two-byte min", "\xC2\x80", 0x0080, 2},
                                                   ValidCase{"c with caron", "č", 0x010D, 2},
                                                   ValidCase{"two-byte max", "\xDF\xBF", 0x07FF, 2},
                                                   ValidCase{"three-byte min", "\xE0\xA0\x80", 0x0800, 3},
                                                   ValidCase{"cjk", "漢", 0x6F22, 3},
                                                   ValidCase{"just below surrogates", "\xED\x9F\xBF", 0xD7FF, 3},
                                                   ValidCase{"just above surrogates", "\xEE\x80\x80", 0xE000, 3},
                                                   ValidCase{"three-byte max", "\xEF\xBF\xBF", 0xFFFF, 3},
                                                   ValidCase{"four-byte min", "\xF0\x90\x80\x80", 0x10000, 4},
                                                   ValidCase{"emoji", "😀", 0x1F600, 4},
                                                   ValidCase{"largest code point", "\xF4\x8F\xBF\xBF", 0x10FFFF, 4}),
                                 // `info` would shadow a parameter inside the macro's own expansion.
                                 [](const ::testing::TestParamInfo<ValidCase>& test_case) {
                                     return sanitised(test_case.param.name);
                                 });

        struct InvalidCase {
            std::string_view name;
            std::string_view input;
            std::size_t offset;  ///< The byte the rejection must name.
        };

        class Utf8InvalidTest : public ::testing::TestWithParam<InvalidCase> {};

        TEST_P(Utf8InvalidTest, IsRejectedAtTheRightByte) {
            const InvalidCase& test = GetParam();

            const Result<DecodedCodePoint> decoded = decode_one(test.input, 0);

            ASSERT_FALSE(decoded) << "decoded to U+" << std::hex << static_cast<std::uint32_t>(decoded->code_point);
            EXPECT_EQ(decoded.error().code, ErrorCode::InvalidUtf8);
            EXPECT_TRUE(decoded.error().context.starts_with("byte " + std::to_string(test.offset) + ":"))
                    << decoded.error().context;
        }

        INSTANTIATE_TEST_SUITE_P(Table, Utf8InvalidTest,
                                 ::testing::Values(
                                         // Overlong forms: the same code point spelled with more bytes
                                         // than it needs, historically used to sneak '/' and NUL past
                                         // filters that only checked the shortest encoding.
                                         InvalidCase{"overlong NUL two bytes", "\xC0\x80", 0},
                                         InvalidCase{"overlong slash two bytes", "\xC1\xAF", 0},
                                         InvalidCase{"overlong three bytes", "\xE0\x80\x80", 0},
                                         InvalidCase{"overlong four bytes", "\xF0\x80\x80\x80", 0},
                                         // Surrogates are UTF-16 plumbing and ill-formed in UTF-8.
                                         InvalidCase{"surrogate D800", "\xED\xA0\x80", 0},
                                         InvalidCase{"surrogate DFFF", "\xED\xBF\xBF", 0},
                                         InvalidCase{"above U+10FFFF", "\xF4\x90\x80\x80", 0},
                                         // Truncation: the offset named is the lead byte, because that
                                         // is where the unfinished sequence starts.
                                         InvalidCase{"truncated two-byte", "\xC3", 0},
                                         InvalidCase{"truncated three-byte", "\xE6\xBC", 0},
                                         InvalidCase{"truncated four-byte", "\xF0\x9F\x98", 0},
                                         // A bad continuation names the byte that is wrong, not the
                                         // start of the sequence.
                                         InvalidCase{"bad continuation at 1", "\xC3\x28", 1},
                                         InvalidCase{"bad continuation at 2", "\xE6\xBC\x28", 2},
                                         InvalidCase{"bad continuation at 3", "\xF0\x9F\x98\x28", 3},
                                         InvalidCase{"lone continuation byte", "\x80", 0},
                                         InvalidCase{"invalid lead FE", "\xFE\x80\x80\x80", 0},
                                         InvalidCase{"invalid lead FF", "\xFF", 0}),
                                 [](const ::testing::TestParamInfo<InvalidCase>& test_case) {
                                     return sanitised(test_case.param.name);
                                 });

        TEST(Utf8DecoderTest, DecodesFromAnyOffset) {
            constexpr std::string_view text = "ač漢";

            const Result<DecodedCodePoint> first = decode_one(text, 0);
            ASSERT_TRUE(first);
            EXPECT_EQ(first->code_point, U'a');

            const Result<DecodedCodePoint> second = decode_one(text, first->length);
            ASSERT_TRUE(second);
            EXPECT_EQ(second->code_point, U'č');

            const Result<DecodedCodePoint> third = decode_one(text, first->length + second->length);
            ASSERT_TRUE(third);
            EXPECT_EQ(third->code_point, U'漢');
        }

        TEST(Utf8DecoderTest, WalksAWholeStringWithoutAllocating) {
            // The decoder is the innermost loop of every text load, and it is called
            // once per code point. An allocation here would be one per character.
            constexpr std::string_view text = "The quick brown fox — čšž 漢字 😀🇭🇷";

            const testing::AllocationGuard guard;

            std::size_t offset = 0;
            std::size_t decoded = 0;
            while (offset < text.size()) {
                const Result<DecodedCodePoint> next = decode_one(text, offset);
                ASSERT_TRUE(next);
                offset += next->length;
                ++decoded;
            }

            EXPECT_EQ(guard.count(), 0U);
            EXPECT_EQ(offset, text.size());
            EXPECT_GT(decoded, 0U);
        }

        TEST(Utf8DecoderTest, TheAllocationCounterActuallyCounts) {
            // Without this, a counter that never increments would make the test above
            // pass for the wrong reason.
            const testing::AllocationGuard guard;

            const std::vector<int> forces_an_allocation(64);

            EXPECT_GE(guard.count(), 1U);
            EXPECT_EQ(forces_an_allocation.size(), 64U);
        }

    }  // namespace
}  // namespace typeit::core
