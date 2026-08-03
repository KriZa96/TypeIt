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

#include <cstddef>

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

    /// How right the typing was, as opposed to how fast (GAMEPLAY section 4.2).
    struct AccuracyMetrics {
        /// Target positions right on the first attempt, over target positions
        /// attempted. A position is attempted once, however many times it is
        /// typed: this is the definition that closes defect C4.
        Accuracy accuracy{0.0};
        /// How much of what stands at the end is right. A run with every
        /// mistake corrected finishes at 1.0 with an accuracy below it — the
        /// two answer different questions, and reporting them separately is
        /// what `Corrected` exists for.
        Accuracy final_correctness{0.0};

        /// The denominators, so a caller can say "3 of 200" rather than only
        /// "98.5%", and so a test can assert the denominator directly.
        std::size_t attempted = 0;
        std::size_t first_attempt_errors = 0;
    };

    /// Pure, and therefore repeatable: the same log gives the same answer
    /// however many times it is asked, which is the property that makes
    /// recomputing a metric after the fact meaningful.
    ///
    /// An empty log gives zeros rather than a NaN. Positions the typist never
    /// attempted — skipped by a space — are in neither the numerator nor the
    /// denominator: not typing something is not the same as typing it wrong,
    /// and TI-041 is where skipping gets its own accounting.
    [[nodiscard]] AccuracyMetrics accuracy(const KeystrokeLog& log, const TextBuffer& target);

    /// `100 × (1 − σ/μ)` over the per-second gross WPM samples, floored at 0
    /// (GAMEPLAY section 4.3 — the widely used definition).
    ///
    /// A second with no keystrokes in it is a sample of 0 WPM, not a sample
    /// that is skipped. That is the whole difference between a metric that
    /// notices a pause and one that ignores it.
    ///
    /// Returned on the published 0–100 scale rather than as a ratio, because
    /// this metric is only ever quoted that way. An empty log scores 0; a run
    /// shorter than one bucket scores 100, having varied in nothing.
    [[nodiscard]] double consistency(const KeystrokeLog& log, const TextBuffer& target);

}  // namespace typeit::core

#endif  // TYPEIT_CORE_METRICS_METRICS_H
