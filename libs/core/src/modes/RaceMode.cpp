#include "typeit/core/modes/RaceMode.h"

#include <algorithm>
#include <cstddef>
#include <ranges>
#include <span>
#include <utility>

#include "typeit/core/modes/IMode.h"
#include "typeit/core/race/DifficultyController.h"
#include "typeit/core/race/Pacer.h"
#include "typeit/core/race/RaceParams.h"
#include "typeit/core/session/Keystroke.h"
#include "typeit/core/session/TypingModel.h"
#include "typeit/core/util/Units.h"

namespace typeit::core {
    namespace {

        /// Whether the position just typed was right on the attempt that
        /// counts.
        ///
        /// `Corrected` is a miss here, as it is everywhere else in this
        /// project: the accuracy the gate reads is first-attempt accuracy, and
        /// a race that let somebody backspace their way to a clean gate would
        /// be rewarding exactly the habit it is meant to train out.
        [[nodiscard]] bool was_first_time_correct(const TypingModel& model, std::size_t at) {
            const std::span<const GraphemeState> states = model.states();
            if (at >= states.size()) {
                return false;
            }
            // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-avoid-unchecked-container-access)
            return states[at] == GraphemeState::Correct;
        }

    }  // namespace

    RaceMode::RaceMode(RaceParams params, Wpm start, bool paced) :
        params_{params}, controller_{params, start},
        // The opening grace is the pacer's: five seconds of stillness while the
        // typist reads the first line (GAMEPLAY section 3.3). Endless has no
        // ghost to hold, so it gets none.
        pacer_{controller_.speed(), paced ? Millis{5'000} : Millis{0}}, paced_{paced} {
        progress_.lives_left = params_.lives;
        progress_.target_speed = controller_.speed();
    }

    void RaceMode::on_start(Millis at, const TypingModel& model) {
        rolling_.emplace(model.target());
        started_at_ = at;
        now_ = at;
        started_ = true;
        if (paced_) {
            pacer_.advance(at);
        }
    }

    void RaceMode::on_keystroke(const Keystroke& event, const TypingModel& model) {
        if (event.kind == KeystrokeKind::Backspace) {
            // Not an attempt. A backspace is a correction, and counting it
            // would let the gate be moved by deleting rather than by typing.
            return;
        }
        ++progress_.distance;
        record_attempt(was_first_time_correct(model, event.target));
        // The lead has just changed, so the chase is re-evaluated on the
        // keystroke as well as on the tick — otherwise being caught is only
        // noticed at the next frame, and a race at 30 Hz forgives more than one
        // at 60.
        chase(event.at, model);
    }

    void RaceMode::on_tick(Millis now, const TypingModel& model) { chase(now, model); }

    void RaceMode::record_attempt(bool correct) {
        attempts_.push_back(correct);
        correct_ += correct ? 1U : 0U;
        while (attempts_.size() > params_.accuracy_window) {
            correct_ -= attempts_.front() ? 1U : 0U;
            attempts_.pop_front();
        }
    }

    Accuracy RaceMode::rolling_accuracy() const {
        if (attempts_.empty()) {
            // Nothing typed yet is not "nothing correct yet". Opening at zero
            // would have the gate back the speed off before the first key is
            // pressed, which is a race that punishes reading the first line.
            return Accuracy{1.0};
        }
        return Accuracy{static_cast<double>(correct_) / static_cast<double>(attempts_.size())};
    }

    void RaceMode::track_sustained() {
        // The highest level the target speed never dropped below for a whole
        // sustain window — *not* the same number for ten seconds, which a ramp
        // that moves continuously never produces. The first cut asked for
        // equality and recorded a peak of zero for every race ever run.
        //
        // Held rather than reached, because the peak instantaneous value is
        // inflated by any lucky burst, and this number becomes the next race's
        // starting speed (GAMEPLAY section 3.4).
        if (curve_.empty()) {
            return;
        }
        const std::int64_t oldest = curve_.back().at.value - params_.sustain_window.value;
        double lowest = curve_.back().wpm.value;
        bool covered = false;
        for (const PacerSample& sample: std::ranges::reverse_view{curve_}) {
            lowest = std::min(lowest, sample.wpm.value);
            if (sample.at.value <= oldest) {
                // The window is spanned: everything from here to now was at
                // least `lowest`.
                covered = true;
                break;
            }
        }
        if (covered && lowest > progress_.peak_sustained.value) {
            progress_.peak_sustained = Wpm{lowest};
        }
    }

    void RaceMode::sample_pacer(Millis now) {
        // One a second. At sixty frames a second, a sample per tick would make
        // an hour's race a quarter of a million rows nobody plots — and the
        // ghost's speed changes by less than a word a minute between them.
        constexpr Millis kInterval{1'000};
        if (sampled_ && now.value - last_sample_.value < kInterval.value) {
            return;
        }
        sampled_ = true;
        last_sample_ = now;
        curve_.push_back(PacerSample{.at = Millis{now.value - started_at_.value}, .wpm = controller_.speed()});
    }

    void RaceMode::caught(const TypingModel& model) {
        if (progress_.lives_left > 1) {
            --progress_.lives_left;
            pacer_.push_back(model.cursor(), static_cast<std::size_t>(params_.lead_comfort));
            controller_.apply_catch_penalty();
            pacer_.set_speed(controller_.speed());
            behind_ = false;
            // Refreshed here rather than left until the next tick: the HUD
            // should show the ghost where the push-back put it, not where it
            // was a moment before somebody lost a life.
            progress_.lead = pacer_.lead(model.cursor());
            progress_.pacer = pacer_.index();
            progress_.target_speed = controller_.speed();
            return;
        }
        progress_.lives_left = 0;
        finished_ = true;
    }

    void RaceMode::chase(Millis now, const TypingModel& model) {
        if (!started_ || finished_) {
            return;
        }
        const Millis elapsed{now.value - now_.value};
        now_ = Millis{std::max(now_.value, now.value)};
        progress_.elapsed = Millis{now_.value - started_at_.value};
        progress_.accuracy = rolling_accuracy();
        if (rolling_.has_value()) {
            // `now_` rather than `now`: the window's left edge only ever moves
            // forward, so a clock that stepped backwards would get a wrong
            // answer rather than a slow one.
            progress_.rolling_wpm = rolling_->advance(model.log(), now_);
            progress_.peak_rolling_wpm = std::max(progress_.peak_rolling_wpm, progress_.rolling_wpm);
        }

        if (!paced_) {
            // Endless: no ghost, no ramp, no end. Everything else — the rolling
            // accuracy, the elapsed time — is still measured, because the run
            // is still a run.
            return;
        }

        pacer_.advance(now);
        const double lead = pacer_.lead(model.cursor());
        progress_.lead = lead;
        progress_.pacer = pacer_.index();

        // Nothing at all happens during the opening grace — no ramp, and no
        // being caught. The typist has typed nothing and the lead is zero,
        // which is *exactly* the condition for being caught, so a grace that
        // held only the pacer still would end the run three hundred
        // milliseconds into the five seconds meant for reading the first line.
        if (pacer_.waiting()) {
            progress_.target_speed = controller_.speed();
            return;
        }
        {
            controller_.advance(elapsed, lead, progress_.accuracy);
            pacer_.set_speed(controller_.speed());
            sample_pacer(now);
            track_sustained();
        }
        progress_.target_speed = controller_.speed();
        progress_.trend = controller_.trend();

        if (lead > 0.0) {
            // Ahead again: the grace window starts over. This is what makes a
            // single fumbled keystroke survivable and a sustained inability to
            // keep up fatal.
            behind_ = false;
            return;
        }
        if (!behind_) {
            behind_ = true;
            behind_since_ = now;
            return;
        }
        if (now.value - behind_since_.value >= params_.grace.value) {
            caught(model);
        }
    }

    ModeProgress RaceMode::progress() const { return OpenProgress{.elapsed = progress_.elapsed}; }

}  // namespace typeit::core
