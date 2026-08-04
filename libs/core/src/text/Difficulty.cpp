#include "typeit/core/text/Difficulty.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <string>
#include <string_view>

#include "typeit/core/text/TextNormalizer.h"
#include "typeit/core/text/Utf8.h"
#include "typeit/core/util/Result.h"

namespace typeit::core {
    namespace {

        /// The two hundred or so words that make up most of written English,
        /// sorted so the lookup can binary search. Two hundred rather than ten
        /// thousand on purpose: the ratio only has to separate "the cat sat on
        /// the mat" from "an astute scholar meticulously perused a compendium",
        /// and a list nobody can read is a list nobody will fix.
        constexpr std::array<std::string_view, 282> kCommonWords{{
                "a",       "able",      "about",  "after",   "again",    "against",   "all",     "almost",  "along",
                "also",    "always",    "am",     "an",      "and",      "another",   "any",     "are",     "around",
                "as",      "ask",       "at",     "away",    "back",     "bad",       "be",      "because", "been",
                "before",  "began",     "begin",  "being",   "best",     "better",    "between", "big",     "both",
                "boy",     "but",       "by",     "call",    "came",     "can",       "come",    "could",   "day",
                "did",     "different", "do",     "does",    "done",     "door",      "down",    "each",    "early",
                "end",     "enough",    "even",   "ever",    "every",    "eye",       "face",    "fact",    "far",
                "feel",    "few",       "find",   "first",   "five",     "for",       "form",    "found",   "four",
                "friend",  "from",      "get",    "girl",    "give",     "go",        "going",   "good",    "got",
                "great",   "group",     "grow",   "had",     "hand",     "hard",      "has",     "have",    "he",
                "head",    "hear",      "help",   "her",     "here",     "high",      "him",     "his",     "hold",
                "home",    "house",     "how",    "i",       "if",       "in",        "into",    "is",      "it",
                "its",     "just",      "keep",   "kind",    "knew",     "know",      "large",   "last",    "late",
                "learn",   "leave",     "left",   "let",     "life",     "light",     "like",    "little",  "live",
                "long",    "look",      "made",   "make",    "man",      "many",      "may",     "me",      "mean",
                "men",     "might",     "mile",   "mind",    "more",     "most",      "move",    "much",    "must",
                "my",      "name",      "near",   "need",    "never",    "new",       "next",    "night",   "no",
                "not",     "now",       "number", "of",      "off",      "often",     "old",     "on",      "once",
                "one",     "only",      "open",   "or",      "other",    "our",       "out",     "over",    "own",
                "page",    "paper",     "part",   "people",  "place",    "play",      "point",   "put",     "read",
                "real",    "right",     "room",   "run",     "said",     "same",      "saw",     "say",     "school",
                "sea",     "second",    "see",    "seem",    "sentence", "set",       "she",     "should",  "show",
                "side",    "since",     "small",  "so",      "some",     "sometimes", "soon",    "sound",   "state",
                "still",   "stop",      "story",  "such",    "take",     "talk",      "tell",    "than",    "that",
                "the",     "their",     "them",   "then",    "there",    "these",     "they",    "thing",   "think",
                "this",    "those",     "though", "thought", "three",    "through",   "time",    "to",      "together",
                "told",    "too",       "took",   "top",     "toward",   "tree",      "try",     "turn",    "two",
                "under",   "until",     "up",     "upon",    "us",       "use",       "very",    "want",    "was",
                "watch",   "water",     "way",    "we",      "well",     "went",      "were",    "what",    "when",
                "where",   "which",     "while",  "white",   "who",      "whole",     "why",     "will",    "with",
                "without", "word",      "work",   "world",   "would",    "write",     "year",    "yes",     "yet",
                "you",     "young",     "your",
        }};

        /// Where each feature stops changing the score. Empirical, and only
        /// ever as good as the corpora they were fitted against — the three
        /// bundled files are what keeps them honest.
        constexpr double kShortestMeanWord = 3.0;
        constexpr double kLongestMeanWord = 8.0;
        constexpr double kRareWordCeiling = 0.8;
        constexpr double kPunctuationCeiling = 0.10;
        constexpr double kCapitalCeiling = 0.15;
        constexpr double kDigitCeiling = 0.05;
        constexpr double kNonAsciiCeiling = 0.10;

        /// Dyadic on purpose: they add to exactly 1.0 rather than to
        /// 0.9999999999999999, so the static_assert below means what it says.
        constexpr double kMeanWordWeight = 0.25;
        constexpr double kRareWordWeight = 0.25;
        constexpr double kPunctuationWeight = 0.1875;
        constexpr double kCapitalWeight = 0.125;
        constexpr double kDigitWeight = 0.09375;
        constexpr double kNonAsciiWeight = 0.09375;

        static_assert(kMeanWordWeight + kRareWordWeight + kPunctuationWeight + kCapitalWeight + kDigitWeight +
                              kNonAsciiWeight ==
                      1.0);

        constexpr double normalized(double value, double lowest, double highest) {
            return std::clamp((value - lowest) / (highest - lowest), 0.0, 1.0);
        }

        constexpr bool is_ascii_digit(char32_t code_point) { return code_point >= U'0' && code_point <= U'9'; }

        /// A capital is a letter that has somewhere lower to go. Cheaper than a
        /// general-category table and right for every cased script.
        bool is_capital(char32_t code_point) { return to_lowercase(code_point) != code_point; }

        /// The separators the rest of the project splits on, per Whitespace.h.
        constexpr std::u32string_view kSeparators = U" \t\n\v\f\r";

        /// Words are split on whitespace, then stripped of the punctuation
        /// hanging off them and lowercased, so `"Hello,` and `hello` are one
        /// word. Otherwise every text with quotation marks in it scores as
        /// unfamiliar vocabulary.
        std::string word_at(std::u32string_view text, std::size_t& offset) {
            const std::size_t start = text.find_first_not_of(kSeparators, offset);
            if (start == std::u32string_view::npos) {
                offset = text.size();
                return {};
            }
            std::size_t end = text.find_first_of(kSeparators, start);
            if (end == std::u32string_view::npos) {
                end = text.size();
            }
            offset = end;

            std::string word;
            for (const char32_t code_point: text.substr(start, end - start)) {
                if (!is_punctuation(code_point)) {
                    encode_one(to_lowercase(code_point), word);
                }
            }
            return word;
        }

        bool is_common(const std::string& word) {
            return std::ranges::binary_search(kCommonWords, std::string_view{word});
        }

        /// Decoding that gives up rather than reporting: a text that reaches
        /// the scorer has already been through the normalizer, which is where
        /// invalid UTF-8 is rejected with a byte offset. Anything malformed
        /// that got here anyway stops the count instead of failing an advisory
        /// number that nothing depends on.
        std::u32string decode(std::string_view text) {
            std::u32string points;
            points.reserve(text.size());
            for (std::size_t offset = 0; offset < text.size();) {
                const Result<DecodedCodePoint> decoded = decode_one(text, offset);
                if (!decoded) {
                    break;
                }
                points.push_back(decoded->code_point);
                offset += decoded->length;
            }
            return points;
        }

    }  // namespace

    DifficultyFeatures difficulty_features(std::string_view text) {
        const std::u32string points = decode(text);
        if (points.empty()) {
            return {};
        }

        std::size_t words = 0;
        std::size_t rare = 0;
        std::size_t word_characters = 0;
        for (std::size_t offset = 0; offset < points.size();) {
            const std::string word = word_at(points, offset);
            if (word.empty()) {
                continue;
            }
            ++words;
            word_characters += word.size();
            if (!is_common(word)) {
                ++rare;
            }
        }

        std::size_t punctuation = 0;
        std::size_t capitals = 0;
        std::size_t letters = 0;
        std::size_t digits = 0;
        std::size_t non_ascii = 0;
        for (const char32_t code_point: points) {
            punctuation += is_punctuation(code_point) ? 1U : 0U;
            digits += is_ascii_digit(code_point) ? 1U : 0U;
            non_ascii += code_point >= 0x80 ? 1U : 0U;
            // A letter is anything that is not punctuation, a digit or a
            // space. Close enough for a density, and it saves a general
            // category table for a number that is advisory anyway.
            if (!is_punctuation(code_point) && !is_ascii_digit(code_point) && code_point > U' ') {
                ++letters;
                capitals += is_capital(code_point) ? 1U : 0U;
            }
        }

        const auto ratio = [](std::size_t part, std::size_t whole) {
            return whole == 0 ? 0.0 : static_cast<double>(part) / static_cast<double>(whole);
        };

        DifficultyFeatures features;
        features.mean_word_length = normalized(ratio(word_characters, words), kShortestMeanWord, kLongestMeanWord);
        features.rare_word_ratio = normalized(ratio(rare, words), 0.0, kRareWordCeiling);
        features.punctuation_density = normalized(ratio(punctuation, points.size()), 0.0, kPunctuationCeiling);
        features.capital_density = normalized(ratio(capitals, letters), 0.0, kCapitalCeiling);
        features.digit_density = normalized(ratio(digits, points.size()), 0.0, kDigitCeiling);
        features.non_ascii_density = normalized(ratio(non_ascii, points.size()), 0.0, kNonAsciiCeiling);
        return features;
    }

    double difficulty_score(std::string_view text) {
        const DifficultyFeatures features = difficulty_features(text);
        const double weighted =
                (kMeanWordWeight * features.mean_word_length) + (kRareWordWeight * features.rare_word_ratio) +
                (kPunctuationWeight * features.punctuation_density) + (kCapitalWeight * features.capital_density) +
                (kDigitWeight * features.digit_density) + (kNonAsciiWeight * features.non_ascii_density);
        return std::clamp(kMinDifficulty + (weighted * (kMaxDifficulty - kMinDifficulty)), kMinDifficulty,
                          kMaxDifficulty);
    }

}  // namespace typeit::core
