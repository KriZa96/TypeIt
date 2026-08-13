// The progressive ramp (TI-124, GAMEPLAY section 3.3).
//
// The pacer chases, the ramp law decides how fast, and this decides when the
// chase is over. Lives, the grace window, being caught, and the push-back that
// follows.
//
// Endless mode is this with the pacer disabled, and is built that way rather
// than as a second code path — the only difference between "never finishes" and
// "finishes when the ghost catches you" is whether there is a ghost.
#ifndef TYPEIT_CORE_MODES_RACEMODE_H
#define TYPEIT_CORE_MODES_RACEMODE_H

#include <cstddef>
#include <deque>
#include <optional>
#include <span>
#include <string_view>
#include <vector>

#include "typeit/core/metrics/RollingWpm.h"
#include "typeit/core/modes/IMode.h"
#include "typeit/core/race/DifficultyController.h"
#include "typeit/core/race/Pacer.h"
#include "typeit/core/race/RaceParams.h"
#include "typeit/core/session/Keystroke.h"
#include "typeit/core/session/TypingModel.h"
#include "typeit/core/util/Units.h"

namespace typeit::core {

    /// What the race HUD draws, and what the results screen records.
    struct RaceProgress {
        Millis elapsed{0};
        /// Where the ghost is, for the inline marker.
        GraphemeIndex pacer{0};
        /// How far ahead the typist is. Negative while they are behind, which
        /// is an ordinary state for up to `grace`.
        double lead = 0.0;
        Wpm target_speed{0.0};
        RampTrend trend = RampTrend::Holding;
        std::size_t lives_left = 0;
        /// Rolling accuracy over the last `accuracy_window` graphemes — the
        /// number the accuracy gate reads, rather than the run's average.
        Accuracy accuracy{1.0};
        /// The highest speed held for at least `sustain_window`. What a
        /// personal best and the next race's starting speed are taken from,
        /// rather than the peak instantaneous value that any lucky burst
        /// inflates (GAMEPLAY section 3.4).
        Wpm peak_sustained{0.0};
        /// The speed at which accuracy first collapsed, if it has. The seed of
        /// the speed-wall analysis (TI-128).
        Wpm wall{0.0};

        /// Gross WPM over the last fifteen seconds, which is what an endless
        /// run's HUD shows instead of a cumulative average (GAMEPLAY §2.5). In
        /// a run with no end a cumulative average stops responding to what the
        /// typist is doing now — an hour of practice makes the last two minutes
        /// invisible.
        Wpm rolling_wpm{0.0};
        /// The highest that reading reached, which is what an endless run is
        /// scored on since there is nothing to finish.
        Wpm peak_rolling_wpm{0.0};
        /// Graphemes attempted. The other half of an endless run's result: how
        /// far, since there is no "how much of it".
        std::size_t distance = 0;
    };

    /// The target speed at one moment of a race.
    ///
    /// Recorded once a second, which is what the results chart draws the ghost
    /// from. Bounded by the length of the run rather than by the number of
    /// ticks: at sixty frames a second, one sample per frame would be an hour's
    /// race in a quarter of a million rows nobody plots.
    struct PacerSample {
        Millis at{0};
        Wpm wpm{0.0};
    };

    class RaceMode final : public IMode {
    public:
        /// `start` is the target speed the pacer opens at — from history, or
        /// the configured floor (TI-126).
        ///
        /// With `paced = false` the ghost never moves and the run never ends,
        /// which is endless mode (TI-120).
        RaceMode(RaceParams params, Wpm start, bool paced = true);

        void on_start(Millis at, const TypingModel& model) override;
        void on_keystroke(const Keystroke& event, const TypingModel& model) override;
        void on_tick(Millis now, const TypingModel& model) override;

        [[nodiscard]] bool is_finished() const override { return finished_; }

        [[nodiscard]] ModeProgress progress() const override;

        [[nodiscard]] std::string_view id() const override { return paced_ ? "race" : "endless"; }

        /// Everything the HUD and the results screen want, which is more than
        /// `ModeProgress` can carry without becoming a struct of everything.
        [[nodiscard]] const RaceProgress& race() const noexcept { return progress_; }

        [[nodiscard]] Wpm speed() const noexcept { return controller_.speed(); }

        /// The ghost's speed over the run, one sample a second, for the results
        /// chart and for `session_sample.pacer_wpm` (TI-127).
        [[nodiscard]] std::span<const PacerSample> pacer_curve() const noexcept { return curve_; }

        /// The numbers this race was actually run by, whatever the config file
        /// says now. Recorded with the run so a race stays reconstructible
        /// after somebody changes a preset (TI-127).
        [[nodiscard]] const RaceParams& params() const noexcept { return params_; }

    private:
        /// The rolling window the accuracy gate reads.
        void record_attempt(bool correct);
        [[nodiscard]] Accuracy rolling_accuracy() const;

        /// One step of the chase: move the ghost, ask the law, decide whether
        /// the run is over.
        void chase(Millis now, const TypingModel& model);

        /// Being caught: a life, a push-back, and a penalty — or the end.
        void caught(const TypingModel& model);

        void track_sustained();

        /// One sample a second, and one at the moment the ghost starts moving,
        /// so the curve begins where the race does rather than a second later.
        void sample_pacer(Millis now);

        RaceParams params_;
        DifficultyController controller_;
        Pacer pacer_;
        bool paced_;

        RaceProgress progress_;
        /// Built at `on_start`, because it needs the target and a mode is not
        /// handed one until then.
        std::optional<RollingWpm> rolling_;
        Millis started_at_{0};
        Millis now_{0};
        bool started_ = false;
        bool finished_ = false;

        /// Since when the typist has been level with or behind the ghost.
        /// Absent while they are ahead, which is what resets the grace window.
        Millis behind_since_{0};
        bool behind_ = false;

        /// One entry per attempted grapheme, oldest first, capped at the
        /// window. A deque rather than a ring buffer because the window is
        /// fifty and the clarity is worth more than the arithmetic.
        std::deque<bool> attempts_;
        std::size_t correct_ = 0;

        std::vector<PacerSample> curve_;
        Millis last_sample_{0};
        bool sampled_ = false;
    };

}  // namespace typeit::core

#endif  // TYPEIT_CORE_MODES_RACEMODE_H
