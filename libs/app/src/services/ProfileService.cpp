#include "typeit/app/services/ProfileService.h"

#include <algorithm>

#include "typeit/core/config/Config.h"
#include "typeit/core/util/Result.h"
#include "typeit/core/util/Units.h"

namespace typeit::app {

    core::Result<core::Wpm> ProfileService::starting_speed(double factor, core::Wpm floor) const {
        const core::Result<core::Wpm> best = history_->best_sustained_wpm(kStartingSpeedWindow);
        if (!best) {
            return std::unexpected{best.error()};
        }
        // No history is a best of zero, which the floor covers without a
        // special case: max(20, 0.85 × 0) is 20.
        return core::Wpm{std::max(floor.value, factor * best->value)};
    }

    core::Result<core::Wpm> ProfileService::starting_speed(const core::Config& config) const {
        if (config.race.start_policy != "from_history") {
            // A fixed start means what it says. Reading the history anyway and
            // then throwing the answer away would make a setting that turns off
            // adaptation still depend on the database being readable.
            return core::Wpm{static_cast<double>(config.race.start_wpm)};
        }
        return starting_speed(kDefaultStartFactor, core::Wpm{static_cast<double>(config.race.start_wpm)});
    }

}  // namespace typeit::app
