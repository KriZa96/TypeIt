#include "typeit/core/race/DifficultyController.h"

#include <algorithm>

#include "typeit/core/race/RaceParams.h"
#include "typeit/core/util/Units.h"

namespace typeit::core {
    namespace {

        constexpr double kMillisPerSecond = 1'000.0;

        [[nodiscard]] double clamped_unit(double value) { return std::clamp(value, 0.0, 1.0); }

        /// `f(lead)`: how comfortably the typist is winning, from nothing at
        /// `lead_comfort` to everything at `lead_comfort + lead_scale`.
        ///
        /// Saturating at one is what stops a typist two hundred graphemes ahead
        /// accelerating away from a speed they were only briefly able to hold.
        [[nodiscard]] double comfort_factor(const RaceParams& params, double lead) {
            if (params.lead_scale <= 0.0) {
                // A scale of zero means "any lead at all is full comfort".
                // Dividing by it would be a NaN propagating into the speed, and
                // from there into the pacer and the whole run.
                return 1.0;
            }
            return clamped_unit((lead - params.lead_comfort) / params.lead_scale);
        }

        /// `g(A)`: the accuracy gate, from nothing at `A_min` to everything at
        /// perfect.
        [[nodiscard]] double accuracy_factor(const RaceParams& params, Accuracy accuracy) {
            const double headroom = 1.0 - params.min_accuracy.value;
            if (headroom <= 0.0) {
                // `A_min` of 1 means nothing short of perfect earns anything,
                // and perfect earns full rate.
                return accuracy.value >= 1.0 ? 1.0 : 0.0;
            }
            return clamped_unit((accuracy.value - params.min_accuracy.value) / headroom);
        }

        /// Which way the ramp is going, for the HUD to show. A function rather
        /// than a chain of conditionals in place, because the chain is one
        /// clang-tidy rule away from being unreadable and this has a name.
        [[nodiscard]] RampTrend trend_of(double per_second) {
            if (per_second > 0.0) {
                return RampTrend::Climbing;
            }
            if (per_second < 0.0) {
                return RampTrend::BackingOff;
            }
            return RampTrend::Holding;
        }

    }  // namespace

    DifficultyController::DifficultyController(RaceParams params, Wpm start) :
        params_{params}, speed_{Wpm{std::clamp(start.value, params.min_speed.value, params.max_speed.value)}} {}

    double DifficultyController::rate(double lead, Accuracy accuracy) const {
        // The order of these three matters. Accuracy is checked first and
        // alone, so that no lead however large can earn speed while accuracy is
        // below the gate — which is the single property that makes this mode
        // teach typing rather than teach mashing.
        if (accuracy < params_.min_accuracy || lead <= params_.lead_danger) {
            return -params_.ramp_down;
        }
        if (lead < params_.lead_comfort) {
            // The dead band. Exactly zero rather than nearly zero: without the
            // hysteresis, `V` oscillates every tick around the boundary and the
            // pacer visibly stutters.
            return 0.0;
        }
        return params_.ramp_up * comfort_factor(params_, lead) * accuracy_factor(params_, accuracy);
    }

    Wpm DifficultyController::advance(Millis elapsed, double lead, Accuracy accuracy) {
        const double per_second = rate(lead, accuracy);
        trend_ = trend_of(per_second);

        if (elapsed.value <= 0) {
            // Time going backwards, or not at all. The trend still updates —
            // the HUD should say why nothing is happening — but the speed does
            // not, because a clock that jumped is not a run that progressed.
            return speed_;
        }

        const double seconds = static_cast<double>(elapsed.value) / kMillisPerSecond;
        speed_ = Wpm{std::clamp(speed_.value + seconds * per_second, params_.min_speed.value, params_.max_speed.value)};
        return speed_;
    }

    void DifficultyController::apply_catch_penalty() {
        speed_ = Wpm{std::clamp(speed_.value * (1.0 - params_.catch_penalty), params_.min_speed.value,
                                params_.max_speed.value)};
    }

}  // namespace typeit::core
