// A user-perceived character: the unit the whole domain counts in (ADR-004,
// TECHNICAL section 1.4).
//
// The bytes are inline rather than pointed at, so a TextBuffer is one
// contiguous allocation and a Grapheme can be copied, compared and stored
// without touching the heap. Twelve bytes covers every sequence in scope
// (ARCHITECTURE section 6.2): a family emoji with two ZWJs is 11.
#ifndef TYPEIT_CORE_TEXT_GRAPHEME_H
#define TYPEIT_CORE_TEXT_GRAPHEME_H

#include <array>
#include <cstddef>
#include <cstdint>
#include <string_view>
#include <type_traits>

namespace typeit::core {

    struct Grapheme {
        /// The longest cluster this project stores. A longer one is truncated at
        /// the segmentation boundary and reported (TI-028).
        static constexpr std::size_t kMaxBytes = 12;

        std::array<char, kMaxBytes> bytes;
        std::uint8_t length;  ///< Bytes in use, 1..kMaxBytes.
        std::uint8_t width;  ///< Display columns: 0, 1 or 2 (TI-029 fills this in).

        [[nodiscard]] constexpr std::string_view view() const noexcept { return {bytes.data(), length}; }

        /// Compares the bytes in use, never the unused tail: two graphemes built
        /// from the same text are equal whatever was left in the array behind
        /// them.
        [[nodiscard]] constexpr bool operator==(const Grapheme& other) const noexcept {
            return width == other.width && view() == other.view();
        }
    };

    static_assert(sizeof(Grapheme) <= 16, "a Grapheme must stay cheap enough to pass by value");
    static_assert(std::is_trivially_copyable_v<Grapheme>);
    static_assert(std::is_aggregate_v<Grapheme>);

}  // namespace typeit::core

#endif  // TYPEIT_CORE_TEXT_GRAPHEME_H
