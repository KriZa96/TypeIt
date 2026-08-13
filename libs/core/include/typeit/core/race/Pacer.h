// The ghost cursor a race is run against (TI-121, GAMEPLAY section 3.1).
//
// A pacer advances through the text at a target speed, and the gap between it
// and the typist is the *lead* — the one number the whole ramp law reads. That
// makes this the smallest and most load-bearing piece of race mode, so it holds
// nothing it does not need: a position, a speed, and the time it last moved.
//
// No clock. Time arrives as a parameter, which is what lets a ten-minute race
// be tested in microseconds and what stops the pacer drifting when a frame is
// late (ADR-012, TI-026).
#ifndef TYPEIT_CORE_RACE_PACER_H
#define TYPEIT_CORE_RACE_PACER_H

#include <cstddef>

#include "typeit/core/util/Units.h"

namespace typeit::core {

    /// The ghost's target speed at one moment of a race.
    ///
    /// Recorded once a second by the mode, read afterwards by the results
    /// screen and by the speed-wall analysis. It lives here rather than with
    /// the mode so that reading a race back does not mean depending on the
    /// thing that played it.
    struct PacerSample {
        /// Since the run began, not since the epoch.
        Millis at{0};
        Wpm wpm{0.0};
    };

    class Pacer {
    public:
        /// Graphemes per word, the convention every speed in this project uses
        /// (GAMEPLAY section 4). Named here because the pacer converts a WPM
        /// into a distance and would otherwise hide a 5 in an expression.
        static constexpr double kGraphemesPerWord = 5.0;
        static constexpr double kMillisPerMinute = 60'000.0;

        /// Starts stationary at grapheme zero, `grace` before it begins to
        /// move. Five seconds by default (GAMEPLAY section 3.3) so the typist
        /// can read the first line and get going.
        ///
        /// A negative speed is refused by clamping to zero rather than by
        /// failing: the ramp law clamps to `V_min` and nothing else sets this,
        /// so a negative here is a caller's bug and the safe reading of it is
        /// "does not move".
        explicit Pacer(Wpm speed, Millis grace = Millis{5'000});

        /// Time has passed. Advances by `Δt · V · 5 / 60` graphemes, or by
        /// nothing while the grace period is still running.
        ///
        /// Time going backwards advances nothing. A clock that jumps — a
        /// suspend, an NTP step — must not teleport the pacer through the text
        /// and end a run the typist was winning.
        void advance(Millis now);

        /// The new target speed, from this moment on.
        ///
        /// Applied forward only: the distance already covered was covered at
        /// the old speed, and recomputing it would make a mid-run speed change
        /// retroactively move the pacer.
        void set_speed(Wpm speed);

        /// Back to `lead` graphemes behind `player`, after being caught
        /// (GAMEPLAY section 3.3).
        ///
        /// The one operation that may move the pacer *backwards*, which is why
        /// it is a named method rather than a setter: every other path is
        /// monotone, and a test says so.
        void push_back(GraphemeIndex player, std::size_t lead);

        /// Fractional, deliberately. The position is accumulated in floating
        /// point and only rounded when somebody asks where to draw it —
        /// rounding every step instead would lose a fraction of a grapheme per
        /// frame, which at sixty frames a second is a pacer running slow by
        /// more than a word a minute.
        [[nodiscard]] double position() const noexcept { return position_; }

        /// Where to draw it: the grapheme the pacer has reached.
        [[nodiscard]] GraphemeIndex index() const noexcept;

        [[nodiscard]] Wpm speed() const noexcept { return speed_; }

        /// How far ahead the typist is, in graphemes. Negative when the pacer
        /// is ahead, which is the state `grace_ms` is counted over rather than
        /// an error.
        [[nodiscard]] double lead(GraphemeIndex player) const noexcept;

        /// Whether the opening grace period is still holding it still.
        [[nodiscard]] bool waiting() const noexcept { return waiting_; }

    private:
        double position_ = 0.0;
        Wpm speed_;
        Millis grace_;
        Millis last_{0};
        bool started_ = false;
        bool waiting_ = true;
    };

}  // namespace typeit::core

#endif  // TYPEIT_CORE_RACE_PACER_H
