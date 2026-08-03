#include <cstdint>
#include <gtest/gtest.h>
#include <set>
#include <string>
#include <string_view>
#include <vector>

#include "typeit/core/text_supply/ITextProvider.h"
#include "typeit/core/text_supply/Prng.h"
#include "typeit/core/text_supply/WholeTextProvider.h"

namespace typeit::core {
    namespace {

        /// Everything a provider will ever yield, which is what a caller sees
        /// and therefore what a test should compare.
        std::vector<std::string> drain(ITextProvider& provider) {
            std::vector<std::string> chunks;
            while (provider.has_more()) {
                chunks.push_back(provider.next_chunk());
            }
            return chunks;
        }

        TEST(WholeTextProviderTest, YieldsTheWholeTextOnceAndThenReportsExhaustion) {
            constexpr std::string_view text = "the quick brown fox";
            WholeTextProvider provider{text};

            ASSERT_TRUE(provider.has_more());
            EXPECT_EQ(provider.next_chunk(), text);
            EXPECT_FALSE(provider.has_more());
        }

        TEST(WholeTextProviderTest, AskingPastExhaustionGivesNothingRatherThanTheTextAgain) {
            WholeTextProvider provider{"abc"};
            static_cast<void>(provider.next_chunk());

            EXPECT_EQ(provider.next_chunk(), "");
            EXPECT_EQ(provider.next_chunk(), "");
            EXPECT_FALSE(provider.has_more());
        }

        TEST(WholeTextProviderTest, AnEmptyTextYieldsNothingAtAll) {
            WholeTextProvider provider{""};

            EXPECT_FALSE(provider.has_more()) << "no chunk, not one empty chunk";
            EXPECT_EQ(provider.next_chunk(), "");
            EXPECT_TRUE(drain(provider).empty());
        }

        TEST(WholeTextProviderTest, MultiByteTextComesBackByteForByte) {
            constexpr std::string_view text = "čšž 漢字 😀\r\nsecond line";
            WholeTextProvider provider{text};

            EXPECT_EQ(provider.next_chunk(), text);
        }

        TEST(WholeTextProviderTest, TheSeedIsCarriedEvenThoughNothingHereIsRandom) {
            const WholeTextProvider provider{"abc", 20260803};

            EXPECT_EQ(provider.seed(), 20260803U);
        }

        TEST(WholeTextProviderTest, TheRecordedSeedReproducesTheStream) {
            WholeTextProvider first{"the quick brown fox", 12345};
            const std::vector<std::string> original = drain(first);

            WholeTextProvider replay{"the quick brown fox", first.seed()};

            EXPECT_EQ(drain(replay), original);
        }

        TEST(PrngTest, TheSameSeedGivesTheSameSequence) {
            Prng first{99};
            Prng second{99};

            for (int i = 0; i < 100; ++i) {
                ASSERT_EQ(first.next(), second.next()) << "draw " << i;
            }
            EXPECT_EQ(first.seed(), 99U);
        }

        TEST(PrngTest, DifferentSeedsGiveDifferentSequences) {
            Prng first{1};
            Prng second{2};

            EXPECT_NE(first.next(), second.next());
        }

        TEST(PrngTest, BelowStaysInsideItsBound) {
            Prng random{20260803};
            std::set<std::size_t> seen;

            for (int i = 0; i < 1'000; ++i) {
                const std::size_t value = random.below(10);
                ASSERT_LT(value, 10U);
                seen.insert(value);
            }
            EXPECT_EQ(seen.size(), 10U) << "and reaches the whole range";
        }

        TEST(PrngTest, ABoundOfOneIsAlwaysZero) {
            Prng random{7};

            EXPECT_EQ(random.below(1), 0U);
        }

        TEST(PrngDeathTest, ABoundOfZeroIsABug) {
            Prng random{7};

            EXPECT_DEBUG_DEATH(static_cast<void>(random.below(0)), "below zero of them");
        }

        TEST(PrngTest, ARandomSeedIsNotTheSameEveryTime) {
            // Not a test of randomness, which is untestable — a test that the
            // seed is drawn at all rather than being a forgotten constant.
            std::set<std::uint64_t> seeds;
            for (int i = 0; i < 8; ++i) {
                seeds.insert(random_seed());
            }

            EXPECT_GT(seeds.size(), 1U);
        }

    }  // namespace
}  // namespace typeit::core
