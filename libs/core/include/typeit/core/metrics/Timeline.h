// Per-second samples of a run, for the results chart and the `session_sample`
// table (TECHNICAL section 5).
//
// The same bucketing the consistency metric is defined over, so the chart and
// the number underneath it cannot disagree about what happened in a given
// second.
#ifndef TYPEIT_CORE_METRICS_TIMELINE_H
#define TYPEIT_CORE_METRICS_TIMELINE_H

#include <cstddef>
#include <optional>
#include <vector>

#include "typeit/core/session/KeystrokeLog.h"
#include "typeit/core/text/TextBuffer.h"
#include "typeit/core/util/Units.h"

namespace typeit::core {

    struct TimelineSample {
        /// When this bucket opens, on the log's own clock.
        Millis at{0};
        /// Gross WPM of the graphemes typed correctly inside it.
        Wpm wpm{0.0};
        /// Every event in the bucket, backspaces included — so the samples
        /// account for the whole log and nothing quietly falls between two
        /// buckets.
        std::size_t keystrokes = 0;
        /// Graphemes typed here that did not match the target position they
        /// applied to, whether or not they were later corrected. A mistake
        /// belongs to the second it was made in.
        std::size_t errors = 0;
        /// The race ghost's target speed in this bucket, where there was a
        /// ghost. Absent for every other mode, which is what keeps a timed
        /// run's chart from having an empty second series on it (TI-127).
        std::optional<Wpm> pacer_wpm;
    };

    /// Buckets are half-open — `[at, at + bucket)` — so an event on a boundary
    /// belongs to exactly one of them. The last event of a run lands exactly on
    /// the closing boundary and is counted in the bucket it ends, which is what
    /// makes a 30-second run 30 samples rather than 31.
    ///
    /// An empty log has no timeline at all: zero samples, not one empty one. A
    /// run shorter than a bucket is a single sample.
    [[nodiscard]] std::vector<TimelineSample> timeline(const KeystrokeLog& log, const TextBuffer& target,
                                                       Millis bucket = Millis{1'000});

}  // namespace typeit::core

#endif  // TYPEIT_CORE_METRICS_TIMELINE_H
