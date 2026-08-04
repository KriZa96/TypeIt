// UTF-8 decoding that refuses malformed input instead of papering over it.
//
// The current application reads text a `char` at a time, which is why the
// README claims FTXUI cannot handle ćčšđž — the terminal was never the
// problem. Producing U+FFFD here instead of an error would repeat that
// mistake more quietly: a typing test that silently substitutes a character
// scores the user against text they were not shown.
//
// One code point per call, no buffer, no allocation on the success path, so
// the segmenter (TI-028) can drive it without a temporary vector.
#ifndef TYPEIT_CORE_TEXT_UTF8_H
#define TYPEIT_CORE_TEXT_UTF8_H

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>

#include "typeit/core/util/Result.h"

namespace typeit::core {

    struct DecodedCodePoint {
        char32_t code_point;
        std::uint8_t length;  ///< Bytes consumed: 1 to 4.

        friend constexpr bool operator==(const DecodedCodePoint&, const DecodedCodePoint&) = default;
    };

    /// The largest code point Unicode defines.
    inline constexpr char32_t kMaxCodePoint = 0x10FFFF;

    /// Decodes the code point starting at `offset`.
    ///
    /// Precondition: `offset < text.size()`.
    ///
    /// Every rejection is an `ErrorCode::InvalidUtf8` whose context begins
    /// `"byte N: "` with the offset of the byte that made the sequence invalid —
    /// the lead byte for a bad length or an overlong form, and the offending byte
    /// itself for a bad continuation.
    [[nodiscard]] Result<DecodedCodePoint> decode_one(std::string_view text, std::size_t offset);

    /// Appends `code_point` to `out` as UTF-8.
    ///
    /// Precondition: the code point is one `decode_one` could have produced —
    /// at most U+10FFFF and not a surrogate. Anything that came out of the
    /// decoder or the normalization tables qualifies; there is no error return
    /// because there is no caller that could have an invalid one.
    void encode_one(char32_t code_point, std::string& out);

}  // namespace typeit::core

#endif  // TYPEIT_CORE_TEXT_UTF8_H
