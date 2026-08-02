// Joining code points into user-perceived characters.
//
// This is what makes "type any text you want" true. The 1.0 application walks
// text one `char` at a time, so a two-byte č is two characters to type, the
// cursor lands mid-sequence, and the README concluded that FTXUI could not
// render Croatian. The terminal was never the problem.
//
// ============================ SCOPE ============================
//
// Implemented (ARCHITECTURE section 6.2):
//   * combining marks — `e` + U+0301 is one cluster
//   * ZWJ sequences — U+1F468 ZWJ U+1F469 is one cluster
//   * regional indicator pairs — 🇭🇷 is one cluster, 🇭🇷🇩🇪 is two
//   * variation selectors — U+FE0F and the U+E0100 block
//   * CR LF — one cluster, unlike LF CR
//
// Deliberately NOT implemented:
//   * full UAX #29, including its complete break-property tables. The mark
//     table below is the common blocks, not every Mn/Mc/Me in Unicode.
//   * emoji modifiers (skin tone) — U+1F469 U+1F3FD is two clusters here.
//   * prepended concatenation marks, Hangul jamo composition, Indic
//     conjunct breaks.
//   * bidirectional text and complex-script shaping.
//
// The boundary is written down so a bug report about Arabic gets an honest
// answer instead of a silent misrender. A cluster that does not fit in
// Grapheme::kMaxBytes is truncated at a code point boundary and counted, so
// the caller can say so rather than discovering it later.
#ifndef TYPEIT_CORE_TEXT_SEGMENTER_H
#define TYPEIT_CORE_TEXT_SEGMENTER_H

#include <cstddef>
#include <string_view>
#include <vector>

#include "typeit/core/text/Grapheme.h"
#include "typeit/core/util/Result.h"

namespace typeit::core {

    struct Segmentation {
        std::vector<Grapheme> graphemes;

        /// Clusters that did not fit in Grapheme::kMaxBytes and lost their tail.
        /// Zero for every text in scope; non-zero means the round trip below no
        /// longer holds and the caller should say so.
        std::size_t truncated = 0;
    };

    /// Decodes `text` and joins its code points into clusters.
    ///
    /// Fails with `ErrorCode::InvalidUtf8` — naming the byte — if the input is not
    /// valid UTF-8. Never substitutes a replacement character.
    ///
    /// Property: with `truncated == 0`, concatenating every grapheme's `view()`
    /// reproduces `text` byte for byte.
    [[nodiscard]] Result<Segmentation> segment(std::string_view text);

}  // namespace typeit::core

#endif  // TYPEIT_CORE_TEXT_SEGMENTER_H
