// The ramp law (TI-122, GAMEPLAY section 3.2).
//
// The heart of race mode, and the reason the mode is worth having: it keeps the
// target speed just above what the typist can sustain, and lets that band drift
// upward across sessions.
//
//     r =  +k_up · f(lead) · g(A)   if lead >= lead_comfort and A >= A_min
//           0                       if lead_danger < lead < lead_comfort
//          -k_down                  if lead <= lead_danger or A < A_min
//
//     f(lead) = clamp((lead - lead_comfort) / lead_scale, 0, 1)
//     g(A)    = clamp((A - A_min) / (1 - A_min), 0, 1)
//
// Pure arithmetic: no clock, no I/O, no randomness, no state beyond the current
// speed. Δt arrives as a parameter. That is not tidiness — a difficulty ramp is
// exactly the kind of feature that is easy to get subtly and unfalsifiably
// wrong, and the only defence is that every property of it can be asserted
// directly.
#ifndef TYPEIT_CORE_RACE_DIFFICULTYCONTROLLER_H
#define TYPEIT_CORE_RACE_DIFFICULTYCONTROLLER_H

#include <cstdint>

#include "typeit/core/race/RaceParams.h"
#include "typeit/core/util/Units.h"

namespace typeit::core {

    /// Which way the ramp is going, for the HUD to show (TI-125).
    ///
    /// Exposed because the accuracy gate is otherwise invisible: a typist forty
    /// graphemes ahead and gaining nothing deserves to see *why*, and the
    /// controller is the only thing that knows which branch it took.
    enum class RampTrend : std::uint8_t {
        Climbing,
        /// The dead band, or a lead high enough to climb with accuracy holding
        /// it at exactly nothing.
        Holding,
        BackingOff,
    };

    class DifficultyController {
    public:
        DifficultyController(RaceParams params, Wpm start);

        /// The target speed after `elapsed` more time at this lead and this
        /// rolling accuracy.
        ///
        /// `lead` is in graphemes and may be negative — the typist is behind,
        /// which is a state the run continues in for `grace` before the pacer
        /// catches them.
        Wpm advance(Millis elapsed, double lead, Accuracy accuracy);

        [[nodiscard]] Wpm speed() const noexcept { return speed_; }

        [[nodiscard]] RampTrend trend() const noexcept { return trend_; }

        /// What the speed loses on being caught (GAMEPLAY section 3.3).
        void apply_catch_penalty();

        [[nodiscard]] const RaceParams& params() const noexcept { return params_; }

        /// `r`, in WPM per second, for this lead and accuracy. Public because
        /// it is the law itself and every property in the test suite is about
        /// it rather than about the integration on top.
        [[nodiscard]] double rate(double lead, Accuracy accuracy) const;

    private:
        RaceParams params_;
        Wpm speed_;
        RampTrend trend_ = RampTrend::Holding;
    };

}  // namespace typeit::core

#endif  // TYPEIT_CORE_RACE_DIFFICULTYCONTROLLER_H
