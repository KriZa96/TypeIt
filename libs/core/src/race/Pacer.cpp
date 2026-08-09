#include "typeit/core/race/Pacer.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>

#include "typeit/core/util/Units.h"

namespace typeit::core {
    namespace {

        /// A speed the pacer can actually be run at.
        ///
        /// Negative is a caller's bug — the ramp law clamps to `V_min` and
        /// nothing else writes this — and the safe reading of a bug here is
        /// "does not move" rather than "runs backwards through the text".
        [[nodiscard]] Wpm usable(Wpm speed) { return Wpm{std::max(0.0, speed.value)}; }

    }  // namespace

    Pacer::Pacer(Wpm speed, Millis grace) :
        speed_{usable(speed)}, grace_{Millis{std::max<std::int64_t>(0, grace.value)}} {}

    void Pacer::advance(Millis now) {
        if (!started_) {
            // The first tick establishes the origin rather than covering the
            // distance from the epoch to it, which is forty-five thousand years
            // of graphemes.
            started_ = true;
            last_ = now;
            return;
        }

        const std::int64_t elapsed = now.value - last_.value;
        if (elapsed <= 0) {
            // Time going backwards, or not at all. A clock that jumps — a
            // suspend, an NTP step — must not teleport the pacer through the
            // text and end a run the typist was winning.
            return;
        }
        last_ = now;

        if (waiting_) {
            grace_ = Millis{grace_.value - elapsed};
            if (grace_.value > 0) {
                return;
            }
            // The grace ran out part way through this step, so the pacer moves
            // for the remainder of it. Throwing the remainder away would make
            // the start depend on how often somebody happened to call this.
            const std::int64_t moving = -grace_.value;
            waiting_ = false;
            grace_ = Millis{0};
            position_ += static_cast<double>(moving) * speed_.value * kGraphemesPerWord / kMillisPerMinute;
            return;
        }

        position_ += static_cast<double>(elapsed) * speed_.value * kGraphemesPerWord / kMillisPerMinute;
    }

    void Pacer::set_speed(Wpm speed) { speed_ = usable(speed); }

    void Pacer::push_back(GraphemeIndex player, std::size_t lead) {
        // Never past the start of the text: a push-back near the beginning
        // would otherwise put the pacer at a negative position, and every
        // reader of `index()` would have to defend against it.
        const double behind = static_cast<double>(player.value) - static_cast<double>(lead);
        position_ = std::max(0.0, behind);
    }

    GraphemeIndex Pacer::index() const noexcept {
        // Truncated rather than rounded: the pacer has *reached* the grapheme
        // it has fully covered, and rounding up would draw it one ahead of
        // where the lead says it is.
        return GraphemeIndex{static_cast<std::size_t>(position_)};
    }

    double Pacer::lead(GraphemeIndex player) const noexcept { return static_cast<double>(player.value) - position_; }

}  // namespace typeit::core
