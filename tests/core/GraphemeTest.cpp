#include <cstdint>
#include <gtest/gtest.h>
#include <string_view>
#include <type_traits>

#include "typeit/core/text/Grapheme.h"

namespace typeit::core {
    namespace {

        constexpr Grapheme make(std::string_view text, std::uint8_t width = 1) {
            Grapheme grapheme{.bytes = {}, .length = static_cast<std::uint8_t>(text.size()), .width = width};
            for (std::size_t i = 0; i < text.size(); ++i) {
                grapheme.bytes.at(i) = text[i];
            }
            return grapheme;
        }

        static_assert(sizeof(Grapheme) <= 16);
        static_assert(std::is_trivially_copyable_v<Grapheme>);

        TEST(GraphemeTest, ViewExposesOnlyTheBytesInUse) {
            const Grapheme grapheme = make("č");

            EXPECT_EQ(grapheme.view(), "č");
            EXPECT_EQ(grapheme.view().size(), 2U);
            EXPECT_EQ(grapheme.length, 2);
        }

        TEST(GraphemeTest, EqualityComparesBytesNotAddresses) {
            const Grapheme first = make("ž");
            const Grapheme second = make("ž");

            EXPECT_EQ(first, second);
            EXPECT_NE(&first, &second);
        }

        TEST(GraphemeTest, EqualityIgnoresWhateverIsBehindTheUsedBytes) {
            Grapheme clean = make("a");
            Grapheme dirty = make("a");
            dirty.bytes.at(5) = 'X';

            EXPECT_EQ(clean, dirty);
        }

        TEST(GraphemeTest, DifferentTextComparesUnequal) {
            EXPECT_NE(make("a"), make("b"));
            EXPECT_NE(make("a"), make("ab"));
            // é as one code point and as e + combining acute are different byte
            // sequences. Making them equal is normalisation's job (TI-071), not this
            // type's.
            EXPECT_NE(make("é"), make("é"));
        }

        TEST(GraphemeTest, WidthParticipatesInEquality) { EXPECT_NE(make("漢", 2), make("漢", 1)); }

        TEST(GraphemeTest, HoldsTheLongestClusterThatFits) {
            // Two people joined by a ZWJ: 4 + 3 + 4 = 11 bytes, and the longest
            // sequence that fits inline. A three-person family is 18 and is truncated
            // at the segmentation boundary with a report (TI-028) rather than stored.
            constexpr std::string_view couple = "\U0001F468\u200D\U0001F469";
            static_assert(couple.size() == 11);
            static_assert(couple.size() <= Grapheme::kMaxBytes);

            const Grapheme grapheme = make(couple);

            EXPECT_EQ(grapheme.view(), couple);
        }

    }  // namespace
}  // namespace typeit::core
