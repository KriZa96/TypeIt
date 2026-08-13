#include <cstddef>
#include <gtest/gtest.h>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "typeit/core/text/TextBuffer.h"
#include "typeit/core/text_supply/ChunkedProvider.h"
#include "typeit/core/util/Result.h"
#include "typeit/core/util/Units.h"
#include "typeit/testing/Preconditions.h"
#include "typeit/testing/TypingRun.h"

namespace typeit::core {
    namespace {

        std::unique_ptr<ChunkedProvider> provider_of(std::string_view text, ChunkedProvider::Options options) {
            Result<std::unique_ptr<ChunkedProvider>> provider = ChunkedProvider::create(text, options);
            EXPECT_TRUE(provider) << text;
            return std::move(*provider);
        }

        std::vector<std::string> drain(ChunkedProvider& provider) {
            std::vector<std::string> chunks;
            while (provider.has_more()) {
                chunks.push_back(provider.next_chunk());
            }
            return chunks;
        }

        std::size_t graphemes_in(std::string_view text) { return testing::text_of(text).size(); }

        constexpr std::string_view kText =
                "the quick brown fox jumps over the lazy dog and then goes home to sleep for a while";

        TEST(ChunkedProviderTest, ChunksRespectTheConfiguredSize) {
            const std::unique_ptr<ChunkedProvider> provider = provider_of(kText, {.chunk_graphemes = 20});

            for (const std::string& chunk: drain(*provider)) {
                EXPECT_LE(graphemes_in(chunk), 20U) << chunk;
            }
        }

        TEST(ChunkedProviderTest, ChunksNeverSplitAWord) {
            const std::unique_ptr<ChunkedProvider> provider = provider_of(kText, {.chunk_graphemes = 20});
            const std::vector<std::string> chunks = drain(*provider);

            ASSERT_GT(chunks.size(), 1U);
            for (std::size_t i = 0; i + 1 < chunks.size(); ++i) {
                EXPECT_TRUE(chunks[i].ends_with(' ')) << "chunk " << i << ": " << chunks[i];
            }
        }

        TEST(ChunkedProviderTest, TheChunksSumToTheText) {
            const std::unique_ptr<ChunkedProvider> provider = provider_of(kText, {.chunk_graphemes = 13});

            std::string rejoined;
            for (const std::string& chunk: drain(*provider)) {
                rejoined += chunk;
            }

            EXPECT_EQ(rejoined, kText);
        }

        TEST(ChunkedProviderTest, TheFinalChunkMayBeShort) {
            const std::unique_ptr<ChunkedProvider> provider = provider_of(kText, {.chunk_graphemes = 20});
            const std::vector<std::string> chunks = drain(*provider);

            ASSERT_FALSE(chunks.empty());
            EXPECT_LT(graphemes_in(chunks.back()), 20U);
            EXPECT_FALSE(chunks.back().ends_with(' ')) << "the text simply ran out";
        }

        TEST(ChunkedProviderTest, AChunkSizeLargerThanTheTextIsOneChunk) {
            const std::unique_ptr<ChunkedProvider> provider = provider_of(kText, {.chunk_graphemes = 10'000});

            const std::vector<std::string> chunks = drain(*provider);

            ASSERT_EQ(chunks.size(), 1U);
            EXPECT_EQ(chunks.front(), kText);
        }

        TEST(ChunkedProviderTest, AWordLongerThanAWholeChunkIsSplitRatherThanOverflowing) {
            // The documented exception. A chunk that ignored its size would be
            // worse than one that cuts a word nobody can type in one sitting.
            constexpr std::string_view text = "supercalifragilisticexpialidocious";
            const std::unique_ptr<ChunkedProvider> provider = provider_of(text, {.chunk_graphemes = 10});

            const std::vector<std::string> chunks = drain(*provider);

            ASSERT_EQ(chunks.size(), 4U);
            EXPECT_EQ(chunks.front(), "supercalif");
        }

        TEST(ChunkedProviderTest, ResumingFromAnOffsetContinuesAtTheRightGrapheme) {
            std::unique_ptr<ChunkedProvider> uninterrupted = provider_of(kText, {.chunk_graphemes = 20});
            static_cast<void>(uninterrupted->next_chunk());
            const GraphemeIndex bookmark = uninterrupted->offset();
            const std::string second_chunk = uninterrupted->next_chunk();

            const std::unique_ptr<ChunkedProvider> resumed =
                    provider_of(kText, {.chunk_graphemes = 20, .offset = bookmark});

            EXPECT_EQ(resumed->offset(), bookmark);
            EXPECT_EQ(resumed->next_chunk(), second_chunk) << "picking the book up where it was put down";
        }

        TEST(ChunkedProviderTest, ABookmarkedRunReadsTheSameTextAsAnUninterruptedOne) {
            const std::unique_ptr<ChunkedProvider> whole = provider_of(kText, {.chunk_graphemes = 17});
            std::string uninterrupted;
            for (const std::string& chunk: drain(*whole)) {
                uninterrupted += chunk;
            }

            // The same text, read one chunk per "session", each session
            // starting from the offset the last one left behind.
            std::string across_sessions;
            GraphemeIndex bookmark{0};
            while (across_sessions.size() < uninterrupted.size()) {
                const std::unique_ptr<ChunkedProvider> session =
                        provider_of(kText, {.chunk_graphemes = 17, .offset = bookmark});
                across_sessions += session->next_chunk();
                bookmark = session->offset();
            }

            EXPECT_EQ(across_sessions, uninterrupted);
        }

        TEST(ChunkedProviderTest, AnOffsetPastTheEndIsClampedAndReported) {
            // A bookmark from a text that has since been re-imported shorter.
            // Refusing to start would be worse than starting at the end and
            // saying so.
            const std::unique_ptr<ChunkedProvider> provider =
                    provider_of(kText, {.chunk_graphemes = 20, .offset = GraphemeIndex{10'000}});

            EXPECT_TRUE(provider->offset_was_clamped());
            EXPECT_EQ(provider->offset(), GraphemeIndex{provider->size()});
            EXPECT_FALSE(provider->has_more());
            EXPECT_EQ(provider->next_chunk(), "");
        }

        TEST(ChunkedProviderTest, AnOffsetInsideTheTextIsNotClamped) {
            const std::unique_ptr<ChunkedProvider> provider =
                    provider_of(kText, {.chunk_graphemes = 20, .offset = GraphemeIndex{5}});

            EXPECT_FALSE(provider->offset_was_clamped());
            EXPECT_TRUE(provider->has_more());
        }

        TEST(ChunkedProviderTest, AnEmptyTextHasNothingToDeliver) {
            const std::unique_ptr<ChunkedProvider> provider = provider_of("", {.chunk_graphemes = 20});

            EXPECT_FALSE(provider->has_more());
            EXPECT_EQ(provider->next_chunk(), "");
            EXPECT_FALSE(provider->offset_was_clamped()) << "offset zero of zero is not past the end";
        }

        TEST(ChunkedProviderTest, ChunksCountGraphemesNotBytes) {
            constexpr std::string_view text = "čšž đšč žćč ćšđ";
            const std::unique_ptr<ChunkedProvider> provider = provider_of(text, {.chunk_graphemes = 4});

            for (const std::string& chunk: drain(*provider)) {
                EXPECT_LE(graphemes_in(chunk), 4U) << chunk;
            }
        }

        TEST(ChunkedProviderTest, InvalidUtf8IsRefusedNamingTheByte) {
            const Result<std::unique_ptr<ChunkedProvider>> provider =
                    ChunkedProvider::create("fine\xE6\xBC", {.chunk_graphemes = 20});

            ASSERT_FALSE(provider);
            EXPECT_EQ(provider.error().code, ErrorCode::InvalidUtf8);
        }

        TEST(ChunkedProviderTest, TheSeedIsCarried) {
            const std::unique_ptr<ChunkedProvider> provider = provider_of(kText, {.chunk_graphemes = 20, .seed = 4242});

            EXPECT_EQ(provider->seed(), 4242U);
        }

        TEST(ChunkedProviderDeathTest, AChunkOfNoGraphemesIsABug) {
            TYPEIT_EXPECT_PRECONDITION(static_cast<void>(ChunkedProvider::create(kText, {.chunk_graphemes = 0})),
                                       "never delivers");
        }

    }  // namespace
}  // namespace typeit::core
