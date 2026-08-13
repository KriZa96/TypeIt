// Where the typist actually ran out of road (TI-128, GAMEPLAY section 3.6).
//
// When a race ends, "you were caught at 84 WPM" is a score. The useful question
// is *why*, and the answer is the band of target speeds in which rolling
// accuracy fell through the gate and did not come back — plus the grapheme
// pairs that failed most often **inside that band**.
//
// Those pairs are, by construction, the keys limiting the typist's speed, as
// opposed to the ones they get wrong while typing comfortably. That distinction
// is the whole feature: a drill built from the whole run's worst pairs teaches
// the mistakes somebody makes anyway, and a drill built from these teaches the
// ones standing between them and going faster.
//
// Pure, and computed after the run rather than during it. Live, "accuracy has
// dropped" is indistinguishable from "accuracy has dropped and will recover in
// two seconds", and only one of those is a wall.
#ifndef TYPEIT_CORE_RACE_SPEEDWALL_H
#define TYPEIT_CORE_RACE_SPEEDWALL_H

#include <cstddef>
#include <optional>
#include <span>
#include <string>
#include <vector>

#include "typeit/core/race/Pacer.h"
#include "typeit/core/race/RaceParams.h"
#include "typeit/core/session/KeystrokeLog.h"
#include "typeit/core/text/TextBuffer.h"
#include "typeit/core/util/Units.h"

namespace typeit::core {

    /// One grapheme typed where another was wanted, and how often.
    struct WallPair {
        std::string expected;
        std::string typed;
        std::size_t count = 0;

        friend bool operator==(const WallPair&, const WallPair&) = default;
    };

    struct SpeedWall {
        /// Absent when accuracy never collapsed — which is a race somebody was
        /// simply caught in, and reporting a wall for it would be inventing a
        /// diagnosis.
        std::optional<Wpm> low;
        std::optional<Wpm> high;

        /// When the collapse began, since the start of the run. Absent with the
        /// band.
        std::optional<Millis> began;

        /// The five worst pairs inside the band, most frequent first. Fewer
        /// than five when there were fewer; empty when the collapse was so
        /// abrupt that nothing was logged inside it, which is sparse data
        /// rather than an error.
        std::vector<WallPair> pairs;

        [[nodiscard]] bool found() const noexcept { return low.has_value(); }
    };

    /// How many pairs a wall reports. Five because it is a list somebody reads
    /// before a drill, not a dataset.
    inline constexpr std::size_t kWallPairs = 5;

    /// The band accuracy collapsed in, and what was failing inside it.
    ///
    /// `curve` is the ghost's speed over the run, from `RaceMode::pacer_curve`.
    /// Without it there is a collapse but no speed to name it at, so the band
    /// comes back empty — a wall with no WPM on it explains nothing.
    [[nodiscard]] SpeedWall speed_wall(const KeystrokeLog& log, const TextBuffer& target,
                                       std::span<const PacerSample> curve, const RaceParams& params);

}  // namespace typeit::core

#endif  // TYPEIT_CORE_RACE_SPEEDWALL_H
