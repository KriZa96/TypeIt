#include "typeit/core/metrics/Metrics.h"

#include <algorithm>
#include <cstddef>
#include <optional>
#include <vector>

#include "typeit/core/session/Keystroke.h"
#include "typeit/core/session/KeystrokeLog.h"
#include "typeit/core/text/Grapheme.h"
#include "typeit/core/text/TextBuffer.h"
#include "typeit/core/util/Units.h"

namespace typeit::core {
    namespace {

        constexpr double kGraphemesPerWord = 5.0;
        constexpr double kMillisPerMinute = 60'000.0;

        struct Tally {
            /// Every grapheme entered, whether it survived or not. Backspaces
            /// are not graphemes and are not counted.
            std::size_t entered = 0;
            /// Positions whose final content matches the target.
            std::size_t correct = 0;
            /// Positions whose final content is there and is wrong. A position
            /// that was typed and then deleted is neither: nothing stands at
            /// it, so there is nothing to be wrong.
            std::size_t uncorrected = 0;
        };

        Tally tally(const KeystrokeLog& log, const TextBuffer& target) {
            // What each position ended up holding. Replaying is what makes a
            // deleted mistake stop counting — the whole point of ADR-002.
            std::vector<std::optional<Grapheme>> final_state(target.size());

            Tally counts;
            for (const Keystroke& event: log.events()) {
                if (event.kind == KeystrokeKind::Character) {
                    ++counts.entered;
                }
                if (event.target >= target.size()) {
                    // A space that ran past the last word resolves against no
                    // position. It was still typed, so it counts as entered.
                    continue;
                }
                // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-avoid-unchecked-container-access) -- bounds checked
                final_state[event.target] =
                        event.kind == KeystrokeKind::Character ? std::optional{event.typed} : std::nullopt;
            }

            for (std::size_t position = 0; position < target.size(); ++position) {
                // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-avoid-unchecked-container-access) -- position < size
                const std::optional<Grapheme>& typed = final_state[position];
                if (!typed.has_value()) {
                    continue;
                }
                if (*typed == target.at(GraphemeIndex{position})) {
                    ++counts.correct;
                } else {
                    ++counts.uncorrected;
                }
            }
            return counts;
        }

    }  // namespace

    SpeedMetrics speed(const KeystrokeLog& log, const TextBuffer& target) {
        const double minutes = static_cast<double>(log.duration().value) / kMillisPerMinute;
        if (minutes <= 0.0) {
            return {};
        }

        const Tally counts = tally(log, target);
        const double raw = static_cast<double>(counts.entered) / kGraphemesPerWord / minutes;
        const double gross = static_cast<double>(counts.correct) / kGraphemesPerWord / minutes;
        const double penalty = static_cast<double>(counts.uncorrected) / minutes;

        return SpeedMetrics{.raw = Wpm{raw}, .gross = Wpm{gross}, .net = Wpm{std::max(0.0, gross - penalty)}};
    }

}  // namespace typeit::core
