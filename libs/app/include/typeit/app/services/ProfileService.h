// Where progression lives (TI-073, GAMEPLAY §3.4).
//
// One number: the speed a race starts at. Today's ceiling becomes tomorrow's
// floor, so nobody re-grinds a speed they have already proven and the ramp
// always begins near the edge of their ability.
//
//     V₀ = max(V_floor, α · best_sustained_wpm over the last 30 days)
//
// `best_sustained_wpm` is the highest speed held for at least the sustain
// window, not the peak instantaneous value — any lucky burst inflates that,
// and a starting speed nobody can hold is a starting speed that loses.
#ifndef TYPEIT_APP_SERVICES_PROFILESERVICE_H
#define TYPEIT_APP_SERVICES_PROFILESERVICE_H

#include "typeit/app/ports/IHistoryRepository.h"
#include "typeit/core/config/Config.h"
#include "typeit/core/util/Result.h"
#include "typeit/core/util/Units.h"

namespace typeit::app {

    /// The published defaults. Both are configurable, and both are named here
    /// rather than written into the formula so that a test can say which one
    /// it is checking.
    inline constexpr double kDefaultStartFactor = 0.85;
    inline constexpr core::Wpm kDefaultStartFloor{20.0};
    inline constexpr core::Days kStartingSpeedWindow{30};

    class ProfileService {
    public:
        /// The repository must outlive the service; the composition root owns
        /// both.
        explicit ProfileService(IHistoryRepository& history) : history_{&history} {}

        /// The speed the next race starts at.
        ///
        /// With no history at all this is the floor: a first-ever run has
        /// nothing to be a fraction of. A repository failure is returned rather
        /// than quietly answered with the floor — a player whose history could
        /// not be read has not gone back to 20 WPM.
        [[nodiscard]] core::Result<core::Wpm> starting_speed(double factor = kDefaultStartFactor,
                                                             core::Wpm floor = kDefaultStartFloor) const;

        /// The same, reading both knobs from the configuration.
        ///
        /// `start_policy` decides whether history is consulted at all: anything
        /// other than `from_history` means the configured `start_wpm` is the
        /// answer and no query is made.
        [[nodiscard]] core::Result<core::Wpm> starting_speed(const core::Config& config) const;

    private:
        IHistoryRepository* history_;
    };

}  // namespace typeit::app

#endif  // TYPEIT_APP_SERVICES_PROFILESERVICE_H
