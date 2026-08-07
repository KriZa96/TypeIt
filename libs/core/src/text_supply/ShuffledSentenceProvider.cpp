#include "typeit/core/text_supply/ShuffledSentenceProvider.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <cstddef>
#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "typeit/core/util/Result.h"

namespace typeit::core {
    namespace {

        /// Abbreviations whose full stop is not the end of a sentence.
        ///
        /// A list rather than a rule, because there is no rule: "Dr." and
        /// "Mr." end in a stop and continue, and "etc." ends in a stop and
        /// usually does not. Getting a rare one wrong costs a split in an odd
        /// place; getting `e.g.` wrong costs one in every technical article
        /// there is.
        constexpr std::array<std::string_view, 14> kAbbreviations{
                "mr", "mrs", "ms", "dr", "prof", "st", "jr", "sr", "vs", "etc", "e.g", "i.e", "fig", "no",
        };

        /// Unchecked on purpose: every index below sits inside the loop that
        /// bounds it. One accessor with one suppression rather than a
        /// suppression on every line — the same argument as `Utf8.cpp`'s
        /// `byte_at` and `TextNormalizer.cpp`'s `at`.
        constexpr char at(std::string_view text, std::size_t index) {
            // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-avoid-unchecked-container-access)
            return text[index];
        }

        [[nodiscard]] bool is_terminator(char letter) { return letter == '.' || letter == '!' || letter == '?'; }

        /// Quotes and brackets that belong to the sentence they close, so
        /// `He said "run."` keeps its quotation mark instead of starting the
        /// next sentence with it.
        [[nodiscard]] bool is_closer(char letter) {
            return letter == '"' || letter == '\'' || letter == ')' || letter == ']' || letter == '\xE2';
        }

        [[nodiscard]] bool is_space(char letter) { return std::isspace(static_cast<unsigned char>(letter)) != 0; }

        /// The word ending just before `stop`, lowercased, for the abbreviation
        /// check. Letters and interior dots only — `e.g` has to come back
        /// whole or the list cannot contain it.
        [[nodiscard]] std::string word_before(std::string_view text, std::size_t stop) {
            std::size_t start = stop;
            while (start > 0) {
                const char letter = at(text, start - 1);
                const bool part_of_word = (std::isalpha(static_cast<unsigned char>(letter)) != 0) || letter == '.';
                if (!part_of_word) {
                    break;
                }
                --start;
            }

            std::string word;
            for (std::size_t index = start; index < stop; ++index) {
                word += static_cast<char>(std::tolower(static_cast<unsigned char>(at(text, index))));
            }
            return word;
        }

        [[nodiscard]] bool ends_an_abbreviation(std::string_view text, std::size_t dot) {
            if (at(text, dot) != '.') {
                // Nobody abbreviates with an exclamation mark.
                return false;
            }
            const std::string word = word_before(text, dot);
            return std::ranges::find(kAbbreviations, word) != kAbbreviations.end();
        }

        [[nodiscard]] std::string trimmed(std::string_view text) {
            std::size_t start = 0;
            std::size_t stop = text.size();
            while (start < stop && is_space(at(text, start))) {
                ++start;
            }
            while (stop > start && is_space(at(text, stop - 1))) {
                --stop;
            }
            return std::string{text.substr(start, stop - start)};
        }

    }  // namespace

    std::vector<std::string> split_sentences(std::string_view text) {
        std::vector<std::string> sentences;
        std::size_t start = 0;

        for (std::size_t cursor = 0; cursor < text.size(); ++cursor) {
            if (!is_terminator(at(text, cursor)) || ends_an_abbreviation(text, cursor)) {
                continue;
            }

            // Take the closers with it: `run."` ends here, not at the quote.
            std::size_t end = cursor + 1;
            while (end < text.size() && is_closer(at(text, end))) {
                ++end;
            }
            // And only split if what follows is whitespace or nothing.
            // `3.14` and `example.com` are not two sentences.
            if (end < text.size() && !is_space(at(text, end))) {
                continue;
            }

            if (std::string sentence = trimmed(text.substr(start, end - start)); !sentence.empty()) {
                sentences.push_back(std::move(sentence));
            }
            start = end;
            cursor = end - 1;
        }

        // Whatever is left after the last terminator. A text with none at all
        // arrives here as one sentence, which is the usual shape of a code
        // snippet and the input this feature is most often given.
        if (std::string tail = trimmed(text.substr(start)); !tail.empty()) {
            sentences.push_back(std::move(tail));
        }
        return sentences;
    }

    Result<std::unique_ptr<ShuffledSentenceProvider>> ShuffledSentenceProvider::create(std::string_view text,
                                                                                       std::uint64_t seed) {
        std::vector<std::string> sentences = split_sentences(text);
        if (sentences.empty()) {
            // An endless provider over nothing is an endless stream of nothing,
            // which hangs a run rather than reporting anything.
            return fail(ErrorCode::EmptyText, "there are no sentences here to shuffle");
        }
        // Not make_unique: the constructor is private, because a provider built
        // from an empty list is not a thing to hand anybody.
        return std::unique_ptr<ShuffledSentenceProvider>{new ShuffledSentenceProvider{std::move(sentences), seed}};
    }

    ShuffledSentenceProvider::ShuffledSentenceProvider(std::vector<std::string> sentences, std::uint64_t seed) :
        sentences_{std::move(sentences)}, prng_{seed} {
        refill();
    }

    void ShuffledSentenceProvider::refill() {
        bag_.resize(sentences_.size());
        for (std::size_t at = 0; at < bag_.size(); ++at) {
            bag_.at(at) = at;
        }

        // Fisher-Yates, written out rather than `std::shuffle`: the standard
        // leaves the mapping from generator output to a permutation
        // unspecified, so two standard libraries would shuffle one seed two
        // ways — and a race replay has to be reproducible on the machine that
        // reads the bug report, not only on the one that wrote it.
        for (std::size_t at = bag_.size(); at > 1; --at) {
            std::swap(bag_.at(at - 1), bag_.at(prng_.below(at)));
        }
    }

    std::string ShuffledSentenceProvider::next_chunk() {
        if (bag_.empty()) {
            // A fresh shuffle each pass rather than the same permutation over
            // and over, which would be memorable after two cycles.
            refill();
        }
        const std::size_t index = bag_.back();
        bag_.pop_back();
        return sentences_.at(index);
    }

}  // namespace typeit::core
