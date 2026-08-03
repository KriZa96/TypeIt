// Speed, derived from the log rather than accumulated during play (ADR-002).
//
// The definitions are GAMEPLAY section 4.1's, and the elapsed time is
// **never clamped**: 1.0 divides by the configured duration rather than by the
// time that actually passed, which is defect C5 and which makes its WPM
// comparable with no other typing test. A run that ends early is fast; a run
// that overruns is slow; both are lies once the denominator is pinned.
//
// Everything here is a pure function of (log, target text). Same inputs, same
// answer, today or in a replay a year from now.
#ifndef TYPEIT_CORE_METRICS_METRICS_H
#define TYPEIT_CORE_METRICS_METRICS_H

#include "typeit/core/session/KeystrokeLog.h"
#include "typeit/core/text/TextBuffer.h"
#include "typeit/core/util/Units.h"

namespace typeit::core {

    /// The three headline speeds. A word is five graphemes throughout
    /// (GAMEPLAY section 4), including the space that starts one, which is what
    /// makes the number comparable with every other typing test.
    struct SpeedMetrics {
        /// Everything entered, mistakes included.
        Wpm raw{0.0};
        /// Only what the finished text got right. The standard headline number.
        Wpm gross{0.0};
        /// Gross, penalised one word per uncorrected error per minute. Floored
        /// at zero: a run can be bad, it cannot be worse than not typing.
        Wpm net{0.0};
    };

    /// Elapsed time is the log's own — first keystroke to last. A log with
    /// nothing in it, or with one event in it, spans no time; the result is all
    /// zeros rather than a division by zero, an infinity or a NaN.
    [[nodiscard]] SpeedMetrics speed(const KeystrokeLog& log, const TextBuffer& target);

}  // namespace typeit::core

#endif  // TYPEIT_CORE_METRICS_METRICS_H
