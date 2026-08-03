// The text, once (GAMEPLAY section 5.4). Quote mode's supply.
#ifndef TYPEIT_CORE_TEXT_SUPPLY_WHOLETEXTPROVIDER_H
#define TYPEIT_CORE_TEXT_SUPPLY_WHOLETEXTPROVIDER_H

#include <cstdint>
#include <string>
#include <string_view>

#include "typeit/core/text_supply/ITextProvider.h"

namespace typeit::core {

    class WholeTextProvider final : public ITextProvider {
    public:
        /// The seed is carried rather than used: nothing here is random, and a
        /// caller that records the seed of every run should not have to care
        /// which providers bother with it.
        explicit WholeTextProvider(std::string_view text, std::uint64_t seed = 0);

        /// The whole text, and after that nothing. An empty text is exhausted
        /// from the outset rather than yielding one empty chunk.
        [[nodiscard]] std::string next_chunk() override;

        [[nodiscard]] bool has_more() const override { return !exhausted_; }

        [[nodiscard]] std::uint64_t seed() const noexcept override { return seed_; }

    private:
        std::string text_;
        std::uint64_t seed_;
        bool exhausted_;
    };

}  // namespace typeit::core

#endif  // TYPEIT_CORE_TEXT_SUPPLY_WHOLETEXTPROVIDER_H
