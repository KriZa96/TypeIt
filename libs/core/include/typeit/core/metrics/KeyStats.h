// Per-grapheme and per-bigram attempts, errors and latency (GAMEPLAY 4.4).
//
// Per-bigram latency is the metric that most directly answers "what should I
// practise": individual keys are rarely slow on their own, transitions are.
// It feeds the results screen, the heatmap and the drill generator, and it maps
// straight onto the `key_stat` and `bigram_stat` tables (TECHNICAL section 5).
#ifndef TYPEIT_CORE_METRICS_KEYSTATS_H
#define TYPEIT_CORE_METRICS_KEYSTATS_H

#include <cstddef>
#include <map>
#include <string>

#include "typeit/core/session/KeystrokeLog.h"
#include "typeit/core/text/TextBuffer.h"
#include "typeit/core/util/Units.h"

namespace typeit::core {

    struct KeyStat {
        /// Keystrokes aimed at this key. Retyping a key after a correction is
        /// another attempt at *the key* — unlike accuracy (TI-037), which counts
        /// positions and deliberately does not.
        std::size_t attempts = 0;
        std::size_t errors = 0;
        /// Summed intervals, outliers excluded. Kept as a total rather than a
        /// mean so that sessions can be merged by addition, which is what the
        /// lifetime aggregate tables do.
        Millis total_latency{0};
        std::size_t latency_samples = 0;

        [[nodiscard]] Millis mean_latency() const noexcept {
            return latency_samples == 0 ? Millis{0}
                                        : Millis{total_latency.value / static_cast<std::int64_t>(latency_samples)};
        }
    };

    struct KeyStats {
        /// Keyed by the grapheme the text asked for, not the one the typist
        /// produced — "how well do I type č" is a question about č.
        std::map<std::string, KeyStat> per_grapheme;
        /// Keyed by the two expected graphemes concatenated.
        std::map<std::string, KeyStat> per_bigram;
    };

    /// Intervals longer than this are thinking, reading or coffee, not typing,
    /// and are left out of the latency totals. The keystroke still counts as an
    /// attempt: it happened, it was just not a measurement of how fast the
    /// fingers move.
    inline constexpr Millis kLatencyOutlierThreshold{3'000};

    /// A bigram is formed from two **consecutive target positions** typed one
    /// after the other — not from two consecutive keystrokes. A backspace and a
    /// retype land on the same position twice, and inventing a bigram out of
    /// that would report a transition the typist never made.
    ///
    /// Latency is the interval from the previous keystroke; the first keystroke
    /// of a run has no previous one and contributes none.
    [[nodiscard]] KeyStats key_stats(const KeystrokeLog& log, const TextBuffer& target,
                                     Millis outlier_threshold = kLatencyOutlierThreshold);

}  // namespace typeit::core

#endif  // TYPEIT_CORE_METRICS_KEYSTATS_H
