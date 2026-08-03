// Gross WPM over a sliding window, for the live HUD (TECHNICAL section 8.1).
//
// The one metric that is not recomputed from scratch. A HUD tick happens
// sixteen times a second and a long endless run holds a hundred thousand
// events; rescanning the log per tick would spend the whole 5 ms keystroke
// budget (ARCHITECTURE section 6.5) on arithmetic nobody asked to repeat.
//
// So this one carries state: a left index that only ever moves forward, and a
// running count of what is inside the window. Each event is looked at twice in
// the life of a run — once when it enters the window, once when it leaves —
// which is what "amortised O(1) per tick" means here, and what
// `events_visited()` exists to prove.
#ifndef TYPEIT_CORE_METRICS_ROLLINGWPM_H
#define TYPEIT_CORE_METRICS_ROLLINGWPM_H

#include <cstddef>
#include <span>

#include "typeit/core/session/KeystrokeLog.h"
#include "typeit/core/text/Grapheme.h"
#include "typeit/core/text/TextBuffer.h"
#include "typeit/core/util/Units.h"

namespace typeit::core {

    class RollingWpm {
    public:
        /// The default the HUD uses. Long enough to be steady, short enough to
        /// react (GAMEPLAY section 4.1).
        static constexpr Millis kDefaultWindow{15'000};

        /// `target` must outlive this. Ticking is only meaningful against the
        /// log of a run over that text.
        explicit RollingWpm(const TextBuffer& target, Millis window = kDefaultWindow);

        /// The gross WPM of the events in `(now − window, now]`.
        ///
        /// `now` must not go backwards — the left index does not rewind, and a
        /// HUD that asked for the past would get a wrong answer rather than a
        /// slow one. Calls must pass the same log, growing at the end.
        ///
        /// Early in a run the window is not full yet, so the divisor is the
        /// time that has actually passed. That is what makes a window larger
        /// than the whole run report exactly the cumulative gross WPM instead
        /// of a fraction of it.
        [[nodiscard]] Wpm advance(const KeystrokeLog& log, Millis now);

        /// Events entered or left the window since construction. Bounded by
        /// twice the log length however many times `advance` is called; the
        /// test asserts exactly that, because "no rescan" is a claim that stops
        /// being true silently.
        [[nodiscard]] std::size_t events_visited() const noexcept { return visited_; }

        /// Where the window currently starts. Only ever moves forward.
        [[nodiscard]] std::size_t left() const noexcept { return left_; }

    private:
        [[nodiscard]] bool is_correct(std::size_t event_index, const KeystrokeLog& log) const;

        std::span<const Grapheme> target_;
        Millis window_;
        std::size_t left_ = 0;
        std::size_t consumed_ = 0;
        std::size_t correct_ = 0;
        std::size_t visited_ = 0;
        Millis start_{0};
        bool started_ = false;
    };

    /// One named struct rather than two bare durations, because two adjacent
    /// `Millis` parameters are two chances to pass them the wrong way round.
    struct SustainedWindow {
        /// The averaging window each reading is taken over.
        Millis window = RollingWpm::kDefaultWindow;
        /// How long a level has to hold before it counts as a speed rather
        /// than a flourish.
        Millis sustain{10'000};
    };

    /// The highest rolling WPM the typist held for at least `sustain`.
    ///
    /// A two-second flourish is not a speed anyone types at; ten seconds of it
    /// is. Sampled once a second over the run and reported as the best level
    /// that never dropped for a whole `sustain` — a run shorter than that
    /// sustained nothing and scores zero.
    [[nodiscard]] Wpm peak_sustained_wpm(const KeystrokeLog& log, const TextBuffer& target, SustainedWindow over = {});

}  // namespace typeit::core

#endif  // TYPEIT_CORE_METRICS_ROLLINGWPM_H
