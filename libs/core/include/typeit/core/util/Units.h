// Strong types for the domain quantities (ADR-010, TECHNICAL section 1.1).
//
// Each is an aggregate holding one value, so construction is explicit —
// `Wpm w = 5.0;` does not compile, `Wpm w{5.0}` does — and no two of them
// interconvert. That is what makes defect C7 unwritable: an index cannot be
// compared against a count, and `size() - 1` on an empty container cannot
// silently become SIZE_MAX, because the operation does not exist.
//
// Only the arithmetic each quantity legitimately supports is provided.
// Accuracy has none: averaging two accuracies is not an accuracy, and the one
// place that wants to is wrong.
#ifndef TYPEIT_CORE_UTIL_UNITS_H
#define TYPEIT_CORE_UTIL_UNITS_H

#include <cassert>
#include <compare>
#include <cstddef>
#include <cstdint>
#include <type_traits>

namespace typeit::core {

    /// Words per minute, where a word is five characters (GAMEPLAY section 4).
    struct Wpm {
        double value;

        friend constexpr bool operator==(const Wpm&, const Wpm&) = default;
        friend constexpr auto operator<=>(const Wpm&, const Wpm&) = default;
    };

    /// A ratio in [0, 1]. Not a percentage: formatting belongs at the boundary.
    struct Accuracy {
        double value;

        friend constexpr bool operator==(const Accuracy&, const Accuracy&) = default;
        friend constexpr auto operator<=>(const Accuracy&, const Accuracy&) = default;
    };

    /// A duration or a timestamp in milliseconds. Signed, because the difference
    /// between two timestamps is meaningful in both directions.
    struct Millis {
        std::int64_t value;

        constexpr Millis& operator+=(Millis other) {
            value += other.value;
            return *this;
        }

        constexpr Millis& operator-=(Millis other) {
            value -= other.value;
            return *this;
        }

        friend constexpr Millis operator+(Millis lhs, Millis rhs) { return Millis{lhs.value + rhs.value}; }
        friend constexpr Millis operator-(Millis lhs, Millis rhs) { return Millis{lhs.value - rhs.value}; }
        friend constexpr Millis operator-(Millis value) { return Millis{-value.value}; }

        friend constexpr bool operator==(const Millis&, const Millis&) = default;
        friend constexpr auto operator<=>(const Millis&, const Millis&) = default;
    };

    /// A position in a TextBuffer, counted in grapheme clusters rather than bytes
    /// or code points (ADR-004).
    struct GraphemeIndex {
        std::size_t value;

        constexpr GraphemeIndex& operator++() {
            ++value;
            return *this;
        }

        constexpr GraphemeIndex operator++(int) {
            const GraphemeIndex before = *this;
            ++value;
            return before;
        }

        /// Precondition: the index is not already at the start. Wrapping to
        /// SIZE_MAX here is exactly defect C7, so it is a contract violation
        /// rather than a value.
        constexpr GraphemeIndex& operator--() {
            assert(value > 0 && "GraphemeIndex decremented below zero");
            --value;
            return *this;
        }

        constexpr GraphemeIndex operator--(int) {
            const GraphemeIndex before = *this;
            --*this;
            return before;
        }

        friend constexpr bool operator==(const GraphemeIndex&, const GraphemeIndex&) = default;
        friend constexpr auto operator<=>(const GraphemeIndex&, const GraphemeIndex&) = default;
    };

    /// Database identity for a finished session.
    struct SessionId {
        std::int64_t value;

        friend constexpr bool operator==(const SessionId&, const SessionId&) = default;
        friend constexpr auto operator<=>(const SessionId&, const SessionId&) = default;
    };

    /// Database identity for a text in the library.
    struct TextId {
        std::int64_t value;

        friend constexpr bool operator==(const TextId&, const TextId&) = default;
        friend constexpr auto operator<=>(const TextId&, const TextId&) = default;
    };

    // The point of ADR-010 is that it costs nothing at runtime. If any of these
    // ever fails, the wrapper has stopped being free and the decision needs
    // revisiting.
    static_assert(sizeof(Wpm) == sizeof(double));
    static_assert(sizeof(Accuracy) == sizeof(double));
    static_assert(sizeof(Millis) == sizeof(std::int64_t));
    static_assert(sizeof(GraphemeIndex) == sizeof(std::size_t));
    static_assert(sizeof(SessionId) == sizeof(std::int64_t));
    static_assert(sizeof(TextId) == sizeof(std::int64_t));

    static_assert(std::is_trivially_copyable_v<Wpm>);
    static_assert(std::is_trivially_copyable_v<Accuracy>);
    static_assert(std::is_trivially_copyable_v<Millis>);
    static_assert(std::is_trivially_copyable_v<GraphemeIndex>);
    static_assert(std::is_trivially_copyable_v<SessionId>);
    static_assert(std::is_trivially_copyable_v<TextId>);

}  // namespace typeit::core

#endif  // TYPEIT_CORE_UTIL_UNITS_H
