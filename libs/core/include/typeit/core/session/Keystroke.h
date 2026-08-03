// One recorded input event (ADR-002, TECHNICAL section 1.5).
//
// The log of these is the single source of truth for a run: every metric is a
// pure function over the events plus the target text, and nothing is
// accumulated while playing. That is what makes defect C4 — accuracy that a
// backspace permanently damages — unrepresentable rather than merely fixed.
#ifndef TYPEIT_CORE_SESSION_KEYSTROKE_H
#define TYPEIT_CORE_SESSION_KEYSTROKE_H

#include <cstdint>
#include <type_traits>

#include "typeit/core/text/Grapheme.h"
#include "typeit/core/util/Units.h"

namespace typeit::core {

    enum class KeystrokeKind : std::uint8_t {
        /// The user produced a grapheme. `typed` holds it.
        Character,
        /// The user deleted one. `typed` is empty; `target` is the position that
        /// was deleted, so the event is meaningful without replaying the log.
        Backspace,
    };

    struct Keystroke {
        /// Milliseconds on whatever clock the session was started with. Only
        /// differences are ever used, so the epoch does not matter.
        Millis at;

        /// The cursor position the event applied to.
        std::uint32_t target;

        KeystrokeKind kind;

        /// What the user produced. Empty (`length == 0`) for a backspace.
        Grapheme typed;

        friend constexpr bool operator==(const Keystroke& lhs, const Keystroke& rhs) noexcept {
            return lhs.at == rhs.at && lhs.target == rhs.target && lhs.kind == rhs.kind && lhs.typed == rhs.typed;
        }
    };

    // ADR-002 estimated ~16 bytes an event, before TI-027 settled on storing a
    // cluster's bytes inline. A Grapheme is 14 of these 32, and buying them back
    // would mean an out-of-line string per event — the allocation that inline
    // storage exists to avoid. 32 bytes is 3.2 MB for the 100k events of a very
    // long run, which is still nothing; the bound is asserted so that a field
    // added without thinking cannot quietly double it.
    static_assert(sizeof(Keystroke) <= 32, "a Keystroke must stay small enough to log one per input");
    static_assert(std::is_trivially_copyable_v<Keystroke>);
    static_assert(std::is_aggregate_v<Keystroke>);

}  // namespace typeit::core

#endif  // TYPEIT_CORE_SESSION_KEYSTROKE_H
