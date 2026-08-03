// One chunk at a time, with a bookmark (GAMEPLAY section 5.4).
//
// What makes a novel a text you type over weeks rather than a text you cannot
// type at all: each session takes the next chunk, and the offset it stopped at
// is a number small enough to keep in the database.
#ifndef TYPEIT_CORE_TEXT_SUPPLY_CHUNKEDPROVIDER_H
#define TYPEIT_CORE_TEXT_SUPPLY_CHUNKEDPROVIDER_H

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <string_view>

#include "typeit/core/text/TextBuffer.h"
#include "typeit/core/text_supply/ITextProvider.h"
#include "typeit/core/util/Result.h"
#include "typeit/core/util/Units.h"

namespace typeit::core {

    class ChunkedProvider final : public ITextProvider {
    public:
        struct Options {
            /// Graphemes per chunk, before backing up to a word boundary. The
            /// default is TECHNICAL section 6's `chunk_graphemes`.
            std::size_t chunk_graphemes = 1'200;
            /// Where to resume. A bookmark from a text that has since been
            /// re-imported shorter is clamped rather than refused — see
            /// `offset_was_clamped()`.
            GraphemeIndex offset{0};
            std::uint64_t seed = 0;
        };

        /// Fails only on invalid UTF-8, naming the byte, exactly as
        /// `TextBuffer::from_utf8` does.
        ///
        /// Returned by pointer because a provider is used through the
        /// interface, and the interface is deliberately immovable — a mode
        /// holding a reference to one must not have it moved out from under it.
        [[nodiscard]] static Result<std::unique_ptr<ChunkedProvider>> create(std::string_view text, Options options);

        /// The next chunk, ending on a word boundary wherever one is close
        /// enough. A word longer than a whole chunk is split, because the
        /// alternative is a chunk that does not fit the setting it was given.
        [[nodiscard]] std::string next_chunk() override;

        [[nodiscard]] bool has_more() const override { return offset_ < text_.size(); }

        [[nodiscard]] std::uint64_t seed() const noexcept override { return seed_; }

        /// Where the next chunk starts — the bookmark to store.
        [[nodiscard]] GraphemeIndex offset() const noexcept { return GraphemeIndex{offset_}; }

        /// The offset asked for was past the end of the text and was moved to
        /// the end. The run is over before it starts, and the caller has a way
        /// to say so instead of silently starting at the beginning.
        [[nodiscard]] bool offset_was_clamped() const noexcept { return offset_was_clamped_; }

        [[nodiscard]] std::size_t size() const noexcept { return text_.size(); }

    private:
        ChunkedProvider(TextBuffer text, Options options, bool clamped);

        TextBuffer text_;
        std::size_t chunk_graphemes_;
        std::size_t offset_;
        std::uint64_t seed_;
        bool offset_was_clamped_;
    };

}  // namespace typeit::core

#endif  // TYPEIT_CORE_TEXT_SUPPLY_CHUNKEDPROVIDER_H
