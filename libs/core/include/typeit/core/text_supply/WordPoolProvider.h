// An endless stream in a text's own style (TI-117, TECHNICAL §8.5).
//
// The provider that makes "any text" compose with "infinite mode": import a
// Rust manual and race against endless Rust-manual vocabulary. Without it,
// endless and race modes would be stuck with a fixed word list and importing a
// text would only ever mean typing that text once.
//
// **Sampling with replacement, weighted by frequency.** Not a shuffle bag:
// a stream in a text's style should say "the" as often as the text does, and a
// bag would say it exactly as many times as the text did and then not at all.
// That is the opposite of the point.
#ifndef TYPEIT_CORE_TEXT_SUPPLY_WORDPOOLPROVIDER_H
#define TYPEIT_CORE_TEXT_SUPPLY_WORDPOOLPROVIDER_H

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "typeit/core/text_supply/ITextProvider.h"
#include "typeit/core/text_supply/Prng.h"
#include "typeit/core/util/Result.h"

namespace typeit::core {

    /// One word and how often the source used it.
    struct WordWeight {
        std::string word;
        std::size_t count = 0;
    };

    struct WordPoolOptions {
        /// Words shorter than this are dropped. Two by default: a pool of "a",
        /// "I" and "of" generates a stream nobody learns anything from, and the
        /// long tail is where a text's character actually lives.
        std::size_t minimum_length = 2;

        /// Keep the source's punctuation on the words that carried it, so
        /// generated text has commas and full stops in roughly the proportion
        /// the source did. Off, every word is bare.
        bool punctuation = true;

        /// Keep capitals as the source wrote them. Off, everything is
        /// lowercased — which is a different practice exercise, not a broken
        /// one.
        bool capitalisation = true;

        /// Words per chunk. A chunk is a line of practice, not a sentence.
        std::size_t words_per_chunk = 12;
    };

    /// The vocabulary of `text`, with counts, in first-appearance order.
    ///
    /// Order matters: a `std::map` would sort alphabetically and make the
    /// weighted pick depend on the alphabet rather than on the text, which is
    /// harmless until somebody compares two runs and finds the seeds no longer
    /// mean the same thing.
    [[nodiscard]] std::vector<WordWeight> word_frequencies(std::string_view text, WordPoolOptions options = {});

    class WordPoolProvider final : public ITextProvider {
    public:
        /// Fails with `EmptyText` when nothing survives tokenising: a pool of
        /// no words is an endless stream of nothing, which hangs a run rather
        /// than reporting anything.
        [[nodiscard]] static Result<std::unique_ptr<WordPoolProvider>> create(std::string_view text, std::uint64_t seed,
                                                                              WordPoolOptions options = {});

        [[nodiscard]] std::string next_chunk() override;
        [[nodiscard]] bool has_more() const override { return true; }
        [[nodiscard]] std::uint64_t seed() const noexcept override { return prng_.seed(); }

        /// The pool, for a test that wants to check the weights rather than
        /// infer them from ten thousand samples.
        [[nodiscard]] const std::vector<WordWeight>& pool() const noexcept { return pool_; }

    private:
        WordPoolProvider(std::vector<WordWeight> pool, std::uint64_t seed, WordPoolOptions options);

        /// One word, drawn with probability proportional to its count.
        [[nodiscard]] const std::string& draw();

        std::vector<WordWeight> pool_;
        /// Running totals, so a draw is a binary search rather than a walk.
        std::vector<std::size_t> cumulative_;
        std::size_t total_ = 0;
        WordPoolOptions options_;
        Prng prng_;
    };

}  // namespace typeit::core

#endif  // TYPEIT_CORE_TEXT_SUPPLY_WORDPOOLPROVIDER_H
