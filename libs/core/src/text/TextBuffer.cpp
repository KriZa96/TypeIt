#include "typeit/core/text/TextBuffer.h"

#include <cassert>
#include <cstddef>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "typeit/core/text/Grapheme.h"
#include "typeit/core/text/Segmenter.h"
#include "typeit/core/text/Whitespace.h"
#include "typeit/core/text/Width.h"
#include "typeit/core/util/Result.h"
#include "typeit/core/util/Units.h"

namespace typeit::core {
    namespace {

        std::size_t count_words(std::span<const Grapheme> graphemes) {
            std::size_t words = 0;
            bool inside_word = false;
            for (const Grapheme& grapheme: graphemes) {
                if (is_word_separator(grapheme)) {
                    inside_word = false;
                } else if (!inside_word) {
                    inside_word = true;
                    ++words;
                }
            }
            return words;
        }

    }  // namespace

    // graphemes_ is initialised first by declaration order, so the two derived
    // counts can read it here.
    TextBuffer::TextBuffer(std::vector<Grapheme> graphemes, std::size_t truncated) :
        graphemes_{std::move(graphemes)}, word_count_{count_words(graphemes_)}, display_width_{total_width(graphemes_)},
        truncated_{truncated} {}

    Result<TextBuffer> TextBuffer::from_utf8(std::string_view text) {
        Result<Segmentation> segmented = segment(text);
        if (!segmented) {
            return std::unexpected{segmented.error()};
        }
        return TextBuffer{std::move(segmented->graphemes), segmented->truncated};
    }

    const Grapheme& TextBuffer::at(GraphemeIndex index) const {
        assert(index.value < graphemes_.size() && "TextBuffer::at index out of range");
        // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-avoid-unchecked-container-access) -- asserted above
        return graphemes_[index.value];
    }

    Result<Grapheme> TextBuffer::at_checked(GraphemeIndex index) const {
        if (index.value >= graphemes_.size()) {
            return fail(ErrorCode::EmptyText,
                        "index " + std::to_string(index.value) + " of " + std::to_string(graphemes_.size()));
        }
        // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-avoid-unchecked-container-access) -- bounds checked above
        return graphemes_[index.value];
    }

    std::string TextBuffer::to_string(GraphemeIndex from, GraphemeIndex to) const {
        assert(from.value <= to.value && "TextBuffer::to_string range is reversed");
        assert(to.value <= graphemes_.size() && "TextBuffer::to_string range ends past the text");

        std::string text;
        for (std::size_t i = from.value; i < to.value; ++i) {
            // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-avoid-unchecked-container-access) -- range asserted above
            text += graphemes_[i].view();
        }
        return text;
    }

}  // namespace typeit::core
