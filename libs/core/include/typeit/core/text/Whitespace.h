// What counts as a word separator, in one place.
//
// The word count, the typing model's word skipping and anything else that has
// an opinion about spaces have to share this definition, or a word boundary
// means one thing to the metrics and another to the cursor.
#ifndef TYPEIT_CORE_TEXT_WHITESPACE_H
#define TYPEIT_CORE_TEXT_WHITESPACE_H

#include <algorithm>

#include "typeit/core/text/Grapheme.h"

namespace typeit::core {

    /// The separators 1.0 counted, which is what `std::istream_iterator<std::string>`
    /// splits on: the six ASCII whitespace characters and nothing else. A
    /// non-breaking space is deliberately absent — not separating words is what
    /// it is for.
    constexpr bool is_ascii_space(char byte) {
        switch (byte) {
            case ' ':
            case '\t':
            case '\n':
            case '\v':
            case '\f':
            case '\r':
                return true;
            default:
                return false;
        }
    }

    /// The whole cluster, not its first byte: CRLF is one cluster of two bytes
    /// and still separates two words.
    [[nodiscard]] constexpr bool is_word_separator(const Grapheme& grapheme) {
        return grapheme.length > 0 && std::ranges::all_of(grapheme.view(), is_ascii_space);
    }

}  // namespace typeit::core

#endif  // TYPEIT_CORE_TEXT_WHITESPACE_H
