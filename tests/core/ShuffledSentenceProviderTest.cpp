// Sentences in random order, forever (TI-116).
//
// Two halves that fail differently. Splitting is wrong when it produces a
// fragment, which reads as a mistake in the *text* and sends somebody looking
// at their file. Shuffling is wrong when it repeats, which reads as nothing at
// all until a typist notices they have stopped reading.

#include <cstddef>
#include <cstdint>
#include <gtest/gtest.h>
#include <memory>
#include <set>
#include <string>
#include <string_view>
#include <vector>

#include "typeit/core/text_supply/ShuffledSentenceProvider.h"
#include "typeit/core/util/Result.h"

namespace typeit::core {
    namespace {

        /// The provider, or a failed test rather than a null dereference.
        std::unique_ptr<ShuffledSentenceProvider> a_provider(std::string_view text, std::uint64_t seed = 42) {
            Result<std::unique_ptr<ShuffledSentenceProvider>> made = ShuffledSentenceProvider::create(text, seed);
            EXPECT_TRUE(made) << (made ? "" : made.error().message);
            return made ? std::move(*made) : nullptr;
        }

        // ---- splitting ---------------------------------------------------------

        TEST(SentenceSplitTest, TheThreeTerminatorsAllEndASentence) {
            const std::vector<std::string> sentences = split_sentences("One. Two! Three? Four.");

            ASSERT_EQ(sentences.size(), 4U);
            EXPECT_EQ(sentences.at(0), "One.");
            EXPECT_EQ(sentences.at(1), "Two!");
            EXPECT_EQ(sentences.at(2), "Three?");
            EXPECT_EQ(sentences.at(3), "Four.");
        }

        TEST(SentenceSplitTest, AClosingQuoteStaysWithTheSentenceItCloses) {
            // `He said "run."` ends at the quote, not before it — otherwise the
            // next sentence begins with a stray `"`.
            const std::vector<std::string> sentences = split_sentences("He said \"run.\" Then he ran.");

            ASSERT_EQ(sentences.size(), 2U);
            EXPECT_EQ(sentences.at(0), "He said \"run.\"");
            EXPECT_EQ(sentences.at(1), "Then he ran.");
        }

        TEST(SentenceSplitTest, CommonAbbreviationsDoNotEndASentence) {
            // Getting `e.g.` wrong costs a split in every technical article
            // there is, which is most of what anybody imports.
            for (const std::string_view text: {"Ask Dr. Smith about it.", "Use a list, e.g. this one.",
                                               "See Mr. and Mrs. Brown.", "Cats, dogs, etc. all count."}) {
                const std::vector<std::string> sentences = split_sentences(text);

                EXPECT_EQ(sentences.size(), 1U) << text << " split into " << sentences.size();
            }
        }

        TEST(SentenceSplitTest, ADecimalPointIsNotAFullStop) {
            // The stop is only a terminator when whitespace or the end follows
            // it, which is what keeps `3.14` and `example.com` whole.
            const std::vector<std::string> sentences = split_sentences("Pi is 3.14 and the site is example.com now.");

            ASSERT_EQ(sentences.size(), 1U);
        }

        TEST(SentenceSplitTest, TextWithNoTerminatorIsOneSentence) {
            // The usual shape of a code snippet, and the input this feature is
            // most often given. Refusing it would refuse the feature.
            const std::vector<std::string> sentences = split_sentences("fn main() { println!() }");

            ASSERT_EQ(sentences.size(), 1U);
            EXPECT_EQ(sentences.front(), "fn main() { println!() }");
        }

        TEST(SentenceSplitTest, TrailingTextAfterTheLastTerminatorIsKept) {
            const std::vector<std::string> sentences = split_sentences("Done. And then some more");

            ASSERT_EQ(sentences.size(), 2U);
            EXPECT_EQ(sentences.at(1), "And then some more");
        }

        TEST(SentenceSplitTest, WhitespaceOnlyInputHasNoSentences) {
            EXPECT_TRUE(split_sentences("   \n\t  ").empty());
            EXPECT_TRUE(split_sentences("").empty());
        }

        // ---- the bag -----------------------------------------------------------

        TEST(ShuffledSentenceProviderTest, NothingToShuffleIsRefusedRatherThanEndlesslyEmpty) {
            // An endless provider over no sentences is an endless stream of
            // nothing, which hangs a run rather than reporting anything.
            const Result<std::unique_ptr<ShuffledSentenceProvider>> made =
                    ShuffledSentenceProvider::create("   \n  ", 1);

            ASSERT_FALSE(made);
            EXPECT_EQ(made.error().code, ErrorCode::EmptyText);
        }

        TEST(ShuffledSentenceProviderTest, TheStreamNeverEnds) {
            const std::unique_ptr<ShuffledSentenceProvider> provider = a_provider("One. Two. Three.");
            ASSERT_NE(provider, nullptr);

            for (int at = 0; at < 100; ++at) {
                EXPECT_TRUE(provider->has_more());
                EXPECT_FALSE(provider->next_chunk().empty()) << "at draw " << at;
            }
            EXPECT_TRUE(provider->has_more()) << "and still, after a hundred";
        }

        TEST(ShuffledSentenceProviderTest, NoSentenceRepeatsUntilThePoolIsExhausted) {
            // The documented shuffle-bag behaviour, and the reason this is not
            // independent sampling: three-in-a-row happens often enough with
            // random draws to be noticed, and a test that repeats itself is one
            // somebody pattern-matches instead of reading.
            const std::unique_ptr<ShuffledSentenceProvider> provider = a_provider("A one. B two. C three. D four.");
            ASSERT_NE(provider, nullptr);
            ASSERT_EQ(provider->size(), 4U);

            for (int pass = 0; pass < 5; ++pass) {
                std::set<std::string> seen;
                for (std::size_t at = 0; at < provider->size(); ++at) {
                    seen.insert(provider->next_chunk());
                }
                EXPECT_EQ(seen.size(), provider->size()) << "pass " << pass << " repeated a sentence";
            }
        }

        TEST(ShuffledSentenceProviderTest, TheSameSeedProducesTheSameOrder) {
            // What "reproduce this run exactly" rests on.
            const std::unique_ptr<ShuffledSentenceProvider> first = a_provider("A. B. C. D. E. F.", 12'345);
            const std::unique_ptr<ShuffledSentenceProvider> second = a_provider("A. B. C. D. E. F.", 12'345);
            ASSERT_NE(first, nullptr);
            ASSERT_NE(second, nullptr);

            for (int at = 0; at < 30; ++at) {
                ASSERT_EQ(first->next_chunk(), second->next_chunk()) << "at draw " << at;
            }
        }

        TEST(ShuffledSentenceProviderTest, DifferentSeedsProduceDifferentOrders) {
            const std::unique_ptr<ShuffledSentenceProvider> first = a_provider("A. B. C. D. E. F. G. H.", 1);
            const std::unique_ptr<ShuffledSentenceProvider> second = a_provider("A. B. C. D. E. F. G. H.", 2);
            ASSERT_NE(first, nullptr);
            ASSERT_NE(second, nullptr);

            bool differed = false;
            for (int at = 0; at < 16 && !differed; ++at) {
                differed = first->next_chunk() != second->next_chunk();
            }
            EXPECT_TRUE(differed) << "two seeds gave the same sixteen draws";
        }

        TEST(ShuffledSentenceProviderTest, TheSeedIsCarriedForTheRecord) {
            const std::unique_ptr<ShuffledSentenceProvider> provider = a_provider("A. B.", 999);
            ASSERT_NE(provider, nullptr);

            EXPECT_EQ(provider->seed(), 999U);
        }

        TEST(ShuffledSentenceProviderTest, ASingleSentenceIsAValidIfDullStream) {
            const std::unique_ptr<ShuffledSentenceProvider> provider = a_provider("Only this one.");
            ASSERT_NE(provider, nullptr);

            EXPECT_EQ(provider->next_chunk(), "Only this one.");
            EXPECT_EQ(provider->next_chunk(), "Only this one.");
            EXPECT_TRUE(provider->has_more());
        }

        TEST(ShuffledSentenceProviderTest, EachPassIsShuffledAgainRatherThanRepeated) {
            // The same permutation over and over would be memorable after two
            // cycles, which is the thing shuffling was for.
            const std::unique_ptr<ShuffledSentenceProvider> provider = a_provider("A. B. C. D. E. F. G. H. I. J.", 7);
            ASSERT_NE(provider, nullptr);

            std::vector<std::string> first_pass;
            std::vector<std::string> second_pass;
            for (std::size_t at = 0; at < provider->size(); ++at) {
                first_pass.push_back(provider->next_chunk());
            }
            for (std::size_t at = 0; at < provider->size(); ++at) {
                second_pass.push_back(provider->next_chunk());
            }

            EXPECT_NE(first_pass, second_pass) << "ten sentences dealt in the same order twice";
        }

    }  // namespace
}  // namespace typeit::core
