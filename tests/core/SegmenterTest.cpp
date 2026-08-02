#include <array>
#include <cctype>
#include <cstddef>
#include <gtest/gtest.h>
#include <numeric>
#include <string>
#include <string_view>
#include <vector>

#include "typeit/core/text/Grapheme.h"
#include "typeit/core/text/Segmenter.h"
#include "typeit/core/util/Result.h"

namespace typeit::core {
    namespace {

        std::string sanitised(std::string_view name) {
            std::string result{name};
            for (char& character: result) {
                if (std::isalnum(static_cast<unsigned char>(character)) == 0) {
                    character = '_';
                }
            }
            return result;
        }

        std::vector<std::string> views_of(const Segmentation& segmentation) {
            std::vector<std::string> views;
            views.reserve(segmentation.graphemes.size());
            for (const Grapheme& grapheme: segmentation.graphemes) {
                views.emplace_back(grapheme.view());
            }
            return views;
        }

        struct Case {
            std::string_view name;
            std::string_view input;
            std::vector<std::string_view> clusters;
        };

        class SegmenterTableTest : public ::testing::TestWithParam<Case> {};

        TEST_P(SegmenterTableTest, SplitsIntoTheExpectedClusters) {
            const Case& test = GetParam();

            const Result<Segmentation> segmented = segment(test.input);

            ASSERT_TRUE(segmented) << to_string(segmented.error());
            EXPECT_EQ(segmented->truncated, 0U);

            const std::vector<std::string> actual = views_of(*segmented);
            ASSERT_EQ(actual.size(), test.clusters.size()) << ::testing::PrintToString(actual);
            for (std::size_t i = 0; i < actual.size(); ++i) {
                EXPECT_EQ(actual[i], test.clusters[i]) << "cluster " << i;
            }
        }

        TEST_P(SegmenterTableTest, RoundTrips) {
            const Case& test = GetParam();

            const Result<Segmentation> segmented = segment(test.input);

            ASSERT_TRUE(segmented);
            const std::string rejoined =
                    std::accumulate(segmented->graphemes.begin(), segmented->graphemes.end(), std::string{},
                                    [](std::string accumulated, const Grapheme& grapheme) {
                                        return accumulated + std::string{grapheme.view()};
                                    });
            EXPECT_EQ(rejoined, test.input);
        }

        INSTANTIATE_TEST_SUITE_P(
                Table, SegmenterTableTest,
                ::testing::Values(Case{"empty", "", {}}, Case{"ascii", "hello", {"h", "e", "l", "l", "o"}},
                                  Case{"space and punctuation", "a, b", {"a", ",", " ", "b"}},
                                  // The characters the README said FTXUI could not render.
                                  Case{"croatian", "čšž", {"č", "š", "ž"}}, Case{"precomposed e acute", "é", {"é"}},
                                  // Same glyph, decomposed: one cluster of three bytes. That it
                                  // is a different cluster from the precomposed form is
                                  // normalisation's problem (TI-071), not segmentation's.
                                  Case{"decomposed e acute", "é", {"é"}}, Case{"two combining marks", "á̈", {"á̈"}},
                                  Case{"combining mark at start", "́a", {"́", "a"}}, Case{"cjk", "漢字", {"漢", "字"}},
                                  Case{"emoji", "😀🙂", {"😀", "🙂"}},
                                  // ZWJ: a couple is one cluster and fits inline at 11 bytes.
                                  Case{"zwj couple", "\U0001F468‍\U0001F469", {"\U0001F468‍\U0001F469"}},
                                  Case{"trailing zwj", "a‍", {"a‍"}},
                                  // Regional indicators pair up, and only pair up.
                                  Case{"one flag", "🇭🇷", {"🇭🇷"}}, Case{"two flags", "🇭🇷🇩🇪", {"🇭🇷", "🇩🇪"}},
                                  Case{"three regional indicators", "🇭🇷🇩", {"🇭🇷", "🇩"}},
                                  Case{"lone regional indicator", "🇭", {"🇭"}},
                                  Case{"flag then letter", "🇭🇷a", {"🇭🇷", "a"}}, Case{"variation selector", "❤️", {"❤️"}},
                                  Case{"keycap", "1️⃣", {"1️⃣"}},
                                  // CRLF is one break; LF CR is two.
                                  Case{"crlf", "a\r\nb", {"a", "\r\n", "b"}},
                                  Case{"lfcr", "a\n\rb", {"a", "\n", "\r", "b"}},
                                  Case{"bare cr", "a\rb", {"a", "\r", "b"}}, Case{"bare lf", "a\nb", {"a", "\n", "b"}},
                                  Case{"tab", "a\tb", {"a", "\t", "b"}},
                                  // Out of scope, asserted so a change of mind is a failing test
                                  // rather than a surprise: a skin-tone modifier does not join.
                                  Case{"skin tone is not joined", "\U0001F469\U0001F3FD", {"\U0001F469", "\U0001F3FD"}},
                                  Case{"mixed scripts", "ač漢😀", {"a", "č", "漢", "😀"}}),
                [](const ::testing::TestParamInfo<Case>& test_case) { return sanitised(test_case.param.name); });

        TEST(SegmenterTest, RejectsInvalidUtf8NamingTheByte) {
            const Result<Segmentation> segmented = segment("ok\xC3\x28");

            ASSERT_FALSE(segmented);
            EXPECT_EQ(segmented.error().code, ErrorCode::InvalidUtf8);
            EXPECT_TRUE(segmented.error().context.starts_with("byte 3:")) << segmented.error().context;
        }

        TEST(SegmenterTest, TruncatesAClusterTooLongToStoreAndSaysSo) {
            // A three-person family: 4 + 3 + 4 + 3 + 4 = 18 bytes, over the 12 a
            // Grapheme holds. TECHNICAL section 1.4 says such a cluster is truncated
            // at the segmentation boundary and reported; this is that report.
            constexpr std::string_view family = "\U0001F468‍\U0001F469‍\U0001F467";
            static_assert(family.size() == 18);

            const Result<Segmentation> segmented = segment(family);

            ASSERT_TRUE(segmented);
            ASSERT_EQ(segmented->graphemes.size(), 1U);
            EXPECT_EQ(segmented->truncated, 1U);
            // Truncation happens on a code point boundary, never mid-sequence.
            EXPECT_EQ(segmented->graphemes.front().view(), "\U0001F468‍\U0001F469");
        }

        TEST(SegmenterTest, TruncationDoesNotLeakIntoTheNextCluster) {
            constexpr std::string_view text = "\U0001F468‍\U0001F469‍\U0001F467a";

            const Result<Segmentation> segmented = segment(text);

            ASSERT_TRUE(segmented);
            ASSERT_EQ(segmented->graphemes.size(), 2U);
            EXPECT_EQ(segmented->truncated, 1U);
            EXPECT_EQ(segmented->graphemes.back().view(), "a");
        }

        TEST(SegmenterTest, RoundTripsACorpusOfMixedScriptLines) {
            // The property that matters: whatever comes in comes back out. A thousand
            // lines built from a pool that mixes every in-scope construct.
            constexpr std::array<std::string_view, 12> pool{
                    "the quick brown fox",
                    "čćšđž ČĆŠĐŽ",
                    "é á̈",
                    "漢字テスト",
                    "😀🙂🎉",
                    "🇭🇷🇩🇪🇬🇧",
                    "❤️",
                    "1️⃣",
                    "\U0001F468‍\U0001F469",
                    "tab\there",
                    "trailing space ",
                    "PUNCT!?;:'\"-",
            };

            std::string corpus;
            for (std::size_t line = 0; line < 1000; ++line) {
                for (std::size_t word = 0; word < 4; ++word) {
                    corpus += pool.at((line * 7 + word * 3) % pool.size());
                    corpus += ' ';
                }
                corpus += "\r\n";
            }

            const Result<Segmentation> segmented = segment(corpus);

            ASSERT_TRUE(segmented) << to_string(segmented.error());
            EXPECT_EQ(segmented->truncated, 0U);

            std::string rejoined;
            rejoined.reserve(corpus.size());
            for (const Grapheme& grapheme: segmented->graphemes) {
                rejoined += grapheme.view();
            }
            EXPECT_EQ(rejoined, corpus);
        }

    }  // namespace
}  // namespace typeit::core
