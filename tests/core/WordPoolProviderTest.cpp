// An endless stream in a text's own style (TI-117).
//
// The determinism tests are the ones that matter most. A race replay has to
// reproduce on the machine reading the bug report, not only on the one that
// wrote it — which is why `Prng::below` is written out rather than handed to
// `std::uniform_int_distribution`, whose mapping onto a range is
// implementation-defined and differs between libstdc++ and libc++.

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <gtest/gtest.h>
#include <map>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "typeit/core/text_supply/WordPoolProvider.h"
#include "typeit/core/util/Result.h"

namespace typeit::core {
    namespace {

        std::unique_ptr<WordPoolProvider> a_pool(std::string_view text, std::uint64_t seed = 42,
                                                 WordPoolOptions options = {}) {
            Result<std::unique_ptr<WordPoolProvider>> made = WordPoolProvider::create(text, seed, options);
            EXPECT_TRUE(made) << (made ? "" : made.error().message);
            return made ? std::move(*made) : nullptr;
        }

        /// How often each word came out, over `draws` chunks.
        std::map<std::string, std::size_t> counted(WordPoolProvider& provider, std::size_t draws) {
            std::map<std::string, std::size_t> seen;
            for (std::size_t at = 0; at < draws; ++at) {
                const std::string chunk = provider.next_chunk();
                std::size_t start = 0;
                while (start <= chunk.size()) {
                    const std::size_t space = chunk.find(' ', start);
                    const std::size_t stop = space == std::string::npos ? chunk.size() : space;
                    if (stop > start) {
                        ++seen[chunk.substr(start, stop - start)];
                    }
                    if (space == std::string::npos) {
                        break;
                    }
                    start = space + 1;
                }
            }
            return seen;
        }

        // ---- the frequency table -----------------------------------------------

        TEST(WordFrequencyTest, TheTableMatchesAHandCount) {
            // "the" three times, "cat" twice, "sat" once — counted by hand,
            // because a table checked against another implementation of itself
            // proves only that they agree.
            const std::vector<WordWeight> pool =
                    word_frequencies("the cat sat the cat and the mat", WordPoolOptions{.minimum_length = 2});

            std::map<std::string, std::size_t> counts;
            for (const WordWeight& entry: pool) {
                counts[entry.word] = entry.count;
            }
            EXPECT_EQ(counts.at("the"), 3U);
            EXPECT_EQ(counts.at("cat"), 2U);
            EXPECT_EQ(counts.at("sat"), 1U);
            EXPECT_EQ(counts.at("and"), 1U);
            EXPECT_EQ(counts.at("mat"), 1U);
            EXPECT_EQ(pool.size(), 5U);
        }

        TEST(WordFrequencyTest, ShortWordsAreDroppedAtTheDocumentedThreshold) {
            // A pool of "a", "I" and "of" generates a stream nobody learns
            // anything from; the long tail is where a text's character lives.
            const std::vector<WordWeight> pool =
                    word_frequencies("a I of the cat", WordPoolOptions{.minimum_length = 3});

            for (const WordWeight& entry: pool) {
                EXPECT_GE(entry.word.size(), 3U) << entry.word;
            }
            EXPECT_EQ(pool.size(), 2U) << "the and cat";
        }

        TEST(WordFrequencyTest, TheOrderIsFirstAppearanceRatherThanAlphabetical) {
            // A `std::map` would make the weighted pick depend on the alphabet
            // rather than on the text, which is harmless right up until two
            // runs are compared and the seeds no longer mean the same thing.
            const std::vector<WordWeight> pool = word_frequencies("zebra apple mango");

            ASSERT_EQ(pool.size(), 3U);
            EXPECT_EQ(pool.at(0).word, "zebra");
            EXPECT_EQ(pool.at(1).word, "apple");
            EXPECT_EQ(pool.at(2).word, "mango");
        }

        TEST(WordFrequencyTest, PunctuationAndCapitalsSurviveWhenAskedFor) {
            const std::vector<WordWeight> kept =
                    word_frequencies("Hello, World!", WordPoolOptions{.punctuation = true, .capitalisation = true});

            ASSERT_EQ(kept.size(), 2U);
            EXPECT_EQ(kept.at(0).word, "Hello,");
            EXPECT_EQ(kept.at(1).word, "World!");
        }

        TEST(WordFrequencyTest, PunctuationAndCapitalsGoWhenTheyAreTurnedOff) {
            const std::vector<WordWeight> bare =
                    word_frequencies("Hello, World!", WordPoolOptions{.punctuation = false, .capitalisation = false});

            ASSERT_EQ(bare.size(), 2U);
            EXPECT_EQ(bare.at(0).word, "hello");
            EXPECT_EQ(bare.at(1).word, "world");
        }

        TEST(WordFrequencyTest, MultiByteCharactersSurviveEitherWay) {
            // `ispunct` is only asked about ASCII bytes, so a continuation byte
            // is never mistaken for punctuation and `č` survives whatever the
            // options say.
            const std::vector<WordWeight> lowered =
                    word_frequencies("Čitanka", WordPoolOptions{.punctuation = false, .capitalisation = false});

            ASSERT_EQ(lowered.size(), 1U);
            EXPECT_NE(lowered.front().word.find("itanka"), std::string::npos) << lowered.front().word;
        }

        // ---- the stream --------------------------------------------------------

        TEST(WordPoolProviderTest, AnEmptySourceIsRefusedBeforeItCanStreamNothing) {
            const Result<std::unique_ptr<WordPoolProvider>> made = WordPoolProvider::create("   \n ", 1);

            ASSERT_FALSE(made);
            EXPECT_EQ(made.error().code, ErrorCode::EmptyText);
        }

        TEST(WordPoolProviderTest, ASourceOfOnlyShortWordsIsAlsoEmpty) {
            // Everything is dropped by the length threshold, so there is no
            // pool — and an endless stream of nothing hangs a run.
            const Result<std::unique_ptr<WordPoolProvider>> made =
                    WordPoolProvider::create("a I", 1, WordPoolOptions{.minimum_length = 4});

            ASSERT_FALSE(made);
            EXPECT_EQ(made.error().code, ErrorCode::EmptyText);
        }

        TEST(WordPoolProviderTest, ASingleWordSourceIsAValidIfDullStream) {
            const std::unique_ptr<WordPoolProvider> provider = a_pool("hello hello hello");
            ASSERT_NE(provider, nullptr);

            EXPECT_TRUE(provider->has_more());
            const std::string chunk = provider->next_chunk();
            EXPECT_FALSE(chunk.empty());
            EXPECT_EQ(chunk.find("hello"), 0U) << chunk;
        }

        TEST(WordPoolProviderTest, TheStreamNeverEnds) {
            const std::unique_ptr<WordPoolProvider> provider = a_pool("one two three four five");
            ASSERT_NE(provider, nullptr);

            for (int at = 0; at < 200; ++at) {
                EXPECT_TRUE(provider->has_more());
                EXPECT_FALSE(provider->next_chunk().empty());
            }
        }

        TEST(WordPoolProviderTest, SamplingIsWeightedByFrequency) {
            // "the" is nine of twelve tokens, so it should be about three
            // quarters of the stream. The bound is loose on purpose: this is a
            // statistical assertion, and a tight one is a test that fails on a
            // Tuesday.
            std::unique_ptr<WordPoolProvider> provider = a_pool("the the the the the the the the the cat dog owl");
            ASSERT_NE(provider, nullptr);

            const std::map<std::string, std::size_t> seen = counted(*provider, 1'000);
            std::size_t total = 0;
            for (const auto& [word, count]: seen) {
                total += count;
            }

            const double share = static_cast<double>(seen.at("the")) / static_cast<double>(total);
            EXPECT_GT(share, 0.65) << "expected about 0.75, got " << share;
            EXPECT_LT(share, 0.85) << "expected about 0.75, got " << share;
        }

        TEST(WordPoolProviderTest, EveryWordInThePoolCanComeOut) {
            // A weighted draw that never reaches the last entry is an
            // off-by-one nobody would see in the ratios.
            std::unique_ptr<WordPoolProvider> provider = a_pool("alpha beta gamma delta epsilon");
            ASSERT_NE(provider, nullptr);

            const std::map<std::string, std::size_t> seen = counted(*provider, 500);

            for (const std::string_view word: {"alpha", "beta", "gamma", "delta", "epsilon"}) {
                EXPECT_TRUE(seen.contains(std::string{word})) << word << " never came out";
            }
        }

        TEST(WordPoolProviderTest, TheSameSeedReproducesTheIdenticalStream) {
            // The acceptance criterion this issue is really about.
            const std::unique_ptr<WordPoolProvider> first = a_pool("the quick brown fox jumps over lazy dogs", 7);
            const std::unique_ptr<WordPoolProvider> second = a_pool("the quick brown fox jumps over lazy dogs", 7);
            ASSERT_NE(first, nullptr);
            ASSERT_NE(second, nullptr);

            for (int at = 0; at < 50; ++at) {
                ASSERT_EQ(first->next_chunk(), second->next_chunk()) << "at chunk " << at;
            }
        }

        TEST(WordPoolProviderTest, TheStreamIsFixedForAKnownSeed) {
            // A pinned value, which is what makes cross-platform determinism a
            // test rather than a hope: `std::mt19937_64` is specified exactly,
            // and `Prng::below` maps its output onto a range by hand for the
            // same reason. If this changes, a recorded replay has broken.
            const std::unique_ptr<WordPoolProvider> provider =
                    a_pool("alpha beta gamma delta", 1, WordPoolOptions{.words_per_chunk = 6});
            ASSERT_NE(provider, nullptr);

            EXPECT_EQ(provider->next_chunk(), "alpha gamma gamma gamma alpha beta");
        }

        TEST(WordPoolProviderTest, DifferentSeedsProduceDifferentStreams) {
            const std::unique_ptr<WordPoolProvider> first = a_pool("the quick brown fox jumps over lazy dogs", 1);
            const std::unique_ptr<WordPoolProvider> second = a_pool("the quick brown fox jumps over lazy dogs", 2);
            ASSERT_NE(first, nullptr);
            ASSERT_NE(second, nullptr);

            EXPECT_NE(first->next_chunk(), second->next_chunk());
        }

        TEST(WordPoolProviderTest, GeneratedChunksNeverSplitAWord) {
            // Separators go between words and never at an edge, so a chunk
            // joined to the next cannot split a word or double a space.
            const std::unique_ptr<WordPoolProvider> provider =
                    a_pool("alpha beta gamma", 5, WordPoolOptions{.words_per_chunk = 4});
            ASSERT_NE(provider, nullptr);

            for (int at = 0; at < 20; ++at) {
                const std::string chunk = provider->next_chunk();

                EXPECT_FALSE(chunk.starts_with(' ')) << chunk;
                EXPECT_FALSE(chunk.ends_with(' ')) << chunk;
                EXPECT_EQ(chunk.find("  "), std::string::npos) << "a doubled space:\n" << chunk;
            }
        }

        TEST(WordPoolProviderTest, AChunkHoldsTheNumberOfWordsItWasAskedFor) {
            const std::unique_ptr<WordPoolProvider> provider =
                    a_pool("alpha beta gamma delta", 3, WordPoolOptions{.words_per_chunk = 7});
            ASSERT_NE(provider, nullptr);

            const std::string chunk = provider->next_chunk();

            EXPECT_EQ(std::ranges::count(chunk, ' '), 6) << chunk;
        }

        TEST(WordPoolProviderTest, TheSeedIsCarriedForTheRecord) {
            const std::unique_ptr<WordPoolProvider> provider = a_pool("alpha beta", 4'242);
            ASSERT_NE(provider, nullptr);

            EXPECT_EQ(provider->seed(), 4'242U);
        }

    }  // namespace
}  // namespace typeit::core
