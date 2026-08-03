// The rules a run is played under (GAMEPLAY section 6, TECHNICAL section 6).
//
// A value type with the documented defaults, so `TypingRules{}` is the game as
// configured out of the box and every variant is one named field away. The
// model applies them; the config layer parses them (TI-050); nothing else needs
// to know they exist.
#ifndef TYPEIT_CORE_SESSION_TYPINGRULES_H
#define TYPEIT_CORE_SESSION_TYPINGRULES_H

#include <cstdint>
#include <type_traits>

namespace typeit::core {

    /// Whether a wrong keystroke blocks further input until it is corrected.
    enum class StopOnError : std::uint8_t {
        Off,
        /// Blocked at the letter: nothing else is accepted until the error goes.
        Letter,
        /// Blocked at the word boundary: finish typing the word however you
        /// like, but you may not leave it with an error still in it.
        Word,
    };

    /// How much of a correction the typist is trusted to want.
    enum class ConfidenceMode : std::uint8_t {
        Off,
        /// No going back over a finished word.
        On,
        /// No going back at all.
        Max,
    };

    struct TypingRules {
        StopOnError stop_on_error = StopOnError::Off;
        bool allow_backspace = true;
        /// A missing space is an error rather than something quietly absorbed.
        bool strict_spaces = true;
        /// A space mid-word jumps to the next word instead of being judged
        /// against the position under the cursor.
        bool space_advances_word = true;
        /// Hide correctness colouring until the run ends. Carried here because
        /// it belongs to the run's rules, but the model does not read it: it is
        /// a rendering decision (Phase 4), and nothing about what was typed
        /// changes because of it.
        bool blind_mode = false;
        ConfidenceMode confidence_mode = ConfidenceMode::Off;

        friend constexpr bool operator==(const TypingRules&, const TypingRules&) = default;
    };

    static_assert(std::is_trivially_copyable_v<TypingRules>);
    static_assert(std::is_aggregate_v<TypingRules>);

}  // namespace typeit::core

#endif  // TYPEIT_CORE_SESSION_TYPINGRULES_H
