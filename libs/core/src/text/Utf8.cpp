#include "typeit/core/text/Utf8.h"

#include <cassert>
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>

#include "typeit/core/util/Result.h"

namespace typeit::core {
    namespace {

        constexpr std::uint8_t byte_at(std::string_view text, std::size_t offset) {
            // Unchecked on purpose: decode_one asserts its precondition once and
            // then bounds-checks the sequence length before reading past the lead
            // byte. at() would throw, and nothing in core throws (ADR-009).
            // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-avoid-unchecked-container-access)
            return static_cast<std::uint8_t>(text[offset]);
        }

        /// Every rejection names the byte that made the sequence invalid, because
        /// "invalid UTF-8 somewhere in a 40 kB file" is not a diagnosis.
        std::unexpected<Error> reject(std::size_t offset, std::string_view reason) {
            return fail(ErrorCode::InvalidUtf8, "byte " + std::to_string(offset) + ": " + std::string{reason});
        }

        constexpr bool is_continuation(std::uint8_t byte) { return (byte & 0xC0U) == 0x80U; }

        /// The smallest code point each length is allowed to encode. Anything below is
        /// an overlong form, which is a classic way to smuggle a '/' or a NUL past a
        /// filter that only looks at the shortest encoding.
        constexpr char32_t smallest_for_length(std::uint8_t length) {
            switch (length) {
                case 2:
                    return 0x80;
                case 3:
                    return 0x800;
                case 4:
                    return 0x10000;
                // Unreachable: the caller has already rejected any length
                // outside 1..4. Defensive, and never exercised.
                default:
                    return 0;
            }
        }

    }  // namespace

    Result<DecodedCodePoint> decode_one(std::string_view text, std::size_t offset) {
        assert(offset < text.size() && "decode_one called past the end of the text");

        const std::uint8_t lead = byte_at(text, offset);

        if (lead < 0x80U) {
            return DecodedCodePoint{.code_point = lead, .length = 1};
        }

        std::uint8_t length = 0;
        char32_t value = 0;
        if ((lead & 0xE0U) == 0xC0U) {
            length = 2;
            value = lead & 0x1FU;
        } else if ((lead & 0xF0U) == 0xE0U) {
            length = 3;
            value = lead & 0x0FU;
        } else if ((lead & 0xF8U) == 0xF0U) {
            length = 4;
            value = lead & 0x07U;
        } else {
            // 0x80-0xBF is a continuation byte with nothing to continue; 0xF8-0xFF
            // was never a legal lead byte in any version of UTF-8.
            return reject(offset, is_continuation(lead) ? "unexpected continuation byte" : "invalid lead byte");
        }

        if (offset + length > text.size()) {
            return reject(offset, "truncated sequence");
        }

        for (std::uint8_t i = 1; i < length; ++i) {
            const std::uint8_t next = byte_at(text, offset + i);
            if (!is_continuation(next)) {
                return reject(offset + i, "invalid continuation byte");
            }
            value = (value << 6U) | (next & 0x3FU);
        }

        if (value < smallest_for_length(length)) {
            return reject(offset, "overlong encoding");
        }

        // Surrogates exist only to let UTF-16 address the astral planes; encoded
        // in UTF-8 they are ill-formed, and accepting them is how a decoder ends
        // up emitting unpaired halves downstream.
        if (value >= 0xD800 && value <= 0xDFFF) {
            return reject(offset, "surrogate code point");
        }

        if (value > kMaxCodePoint) {
            return reject(offset, "code point above U+10FFFF");
        }

        return DecodedCodePoint{.code_point = value, .length = length};
    }

    void encode_one(char32_t code_point, std::string& out) {
        assert(code_point <= kMaxCodePoint && "encode_one called with a code point above U+10FFFF");
        assert((code_point < 0xD800 || code_point > 0xDFFF) && "encode_one called with a surrogate");

        const auto byte = [&out](char32_t value) {
            out.push_back(static_cast<char>(static_cast<std::uint8_t>(value)));
        };

        if (code_point < 0x80) {
            byte(code_point);
        } else if (code_point < 0x800) {
            byte(0xC0U | (code_point >> 6U));
            byte(0x80U | (code_point & 0x3FU));
        } else if (code_point < 0x10000) {
            byte(0xE0U | (code_point >> 12U));
            byte(0x80U | ((code_point >> 6U) & 0x3FU));
            byte(0x80U | (code_point & 0x3FU));
        } else {
            byte(0xF0U | (code_point >> 18U));
            byte(0x80U | ((code_point >> 12U) & 0x3FU));
            byte(0x80U | ((code_point >> 6U) & 0x3FU));
            byte(0x80U | (code_point & 0x3FU));
        }
    }

}  // namespace typeit::core
