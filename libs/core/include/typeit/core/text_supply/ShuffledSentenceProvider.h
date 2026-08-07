// The sentences of a text, in random order, forever (TI-116, GAMEPLAY §5.4).
//
// What makes a single article last an evening. `WholeTextProvider` gives it
// once and stops; this gives it endlessly, in an order nobody can memorise,
// while every sentence stays a real sentence someone wrote.
//
// **A shuffle bag, not independent draws.** Every sentence is used once before
// any is used twice. Sampling at random instead would show the same sentence
// three times in a row often enough to be noticed, and a typing test that
// repeats itself is one somebody stops reading and starts pattern-matching.
#ifndef TYPEIT_CORE_TEXT_SUPPLY_SHUFFLEDSENTENCEPROVIDER_H
#define TYPEIT_CORE_TEXT_SUPPLY_SHUFFLEDSENTENCEPROVIDER_H

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

    /// The sentences of `text`, in the order they were written.
    ///
    /// Split on `.`, `!` and `?` followed by whitespace or the end, with any
    /// closing quotes or brackets kept on the sentence they close. Common
    /// abbreviations — `Dr.`, `e.g.`, `Mr.` — do not end a sentence: splitting
    /// there produces a fragment that reads as a mistake in the text rather
    /// than a mistake in the program.
    ///
    /// A text with no terminator at all is one sentence. That is the usual
    /// shape of a code snippet, and refusing it would refuse the feature its
    /// most common input.
    [[nodiscard]] std::vector<std::string> split_sentences(std::string_view text);

    class ShuffledSentenceProvider final : public ITextProvider {
    public:
        /// Fails with `EmptyText` when there is nothing to shuffle: an endless
        /// provider over no sentences is an endless stream of nothing, which
        /// hangs a run rather than reporting anything.
        [[nodiscard]] static Result<std::unique_ptr<ShuffledSentenceProvider>> create(std::string_view text,
                                                                                      std::uint64_t seed);

        [[nodiscard]] std::string next_chunk() override;
        [[nodiscard]] bool has_more() const override { return true; }
        [[nodiscard]] std::uint64_t seed() const noexcept override { return prng_.seed(); }

        /// How many distinct sentences the bag holds. Exposed because "no
        /// sentence repeats until the pool is exhausted" is a property about
        /// this number, and inferring it from the stream is guesswork.
        [[nodiscard]] std::size_t size() const noexcept { return sentences_.size(); }

    private:
        ShuffledSentenceProvider(std::vector<std::string> sentences, std::uint64_t seed);

        /// Refills and reshuffles the bag. Called when it runs dry, which is
        /// what makes the cycle a cycle rather than a permutation repeated.
        void refill();

        std::vector<std::string> sentences_;
        /// Indices left in this pass, taken from the back.
        std::vector<std::size_t> bag_;
        Prng prng_;
    };

}  // namespace typeit::core

#endif  // TYPEIT_CORE_TEXT_SUPPLY_SHUFFLEDSENTENCEPROVIDER_H
