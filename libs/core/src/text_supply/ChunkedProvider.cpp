#include "typeit/core/text_supply/ChunkedProvider.h"

#include <algorithm>
#include <cassert>
#include <cstddef>
#include <memory>
#include <string>
#include <string_view>
#include <utility>

#include "typeit/core/text/TextBuffer.h"
#include "typeit/core/text/Whitespace.h"
#include "typeit/core/util/Result.h"
#include "typeit/core/util/Units.h"

namespace typeit::core {

    ChunkedProvider::ChunkedProvider(TextBuffer text, Options options, bool clamped) :
        text_{std::move(text)}, chunk_graphemes_{options.chunk_graphemes}, offset_{options.offset.value},
        seed_{options.seed}, offset_was_clamped_{clamped} {}

    Result<std::unique_ptr<ChunkedProvider>> ChunkedProvider::create(std::string_view text, Options options) {
        assert(options.chunk_graphemes > 0 && "a chunk of no graphemes never delivers the text");

        Result<TextBuffer> buffer = TextBuffer::from_utf8(text);
        if (!buffer) {
            return std::unexpected{buffer.error()};
        }

        const bool clamped = options.offset.value > buffer->size();
        if (clamped) {
            options.offset = GraphemeIndex{buffer->size()};
        }
        // Not make_unique: the constructor is private, which is the point.
        return std::unique_ptr<ChunkedProvider>{new ChunkedProvider{std::move(*buffer), options, clamped}};
    }

    std::string ChunkedProvider::next_chunk() {
        if (!has_more()) {
            return {};
        }

        const std::size_t remaining = text_.size() - offset_;
        const std::size_t wanted = std::min(chunk_graphemes_, remaining);
        std::size_t end = offset_ + wanted;

        if (end < text_.size()) {
            // Back up to just after the last separator inside the chunk, so a
            // word is not cut in half between two sessions. A word longer than
            // the whole chunk has no boundary to back up to and is split: a
            // chunk that ignored its size would be worse.
            std::size_t boundary = end;
            while (boundary > offset_ && !is_word_separator(text_.at(GraphemeIndex{boundary - 1}))) {
                --boundary;
            }
            if (boundary > offset_) {
                end = boundary;
            }
        }

        const std::string chunk = text_.to_string(GraphemeIndex{offset_}, GraphemeIndex{end});
        offset_ = end;
        return chunk;
    }

}  // namespace typeit::core
