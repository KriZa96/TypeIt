#include "typeit/core/modes/TimedMode.h"

#include <algorithm>
#include <cassert>

#include "typeit/core/modes/IMode.h"
#include "typeit/core/session/Keystroke.h"
#include "typeit/core/session/TypingModel.h"
#include "typeit/core/util/Units.h"

namespace typeit::core {

    TimedMode::TimedMode(Millis duration) : duration_{duration} {
        assert(duration >= kMinDuration && duration <= kMaxDuration && "duration outside the validated range");
    }

    void TimedMode::on_start(Millis at, const TypingModel& /*model*/) {
        offered_at_ = at;
        now_ = at;
    }

    void TimedMode::on_keystroke(const Keystroke& event, const TypingModel& /*model*/) {
        now_ = std::max(now_, event.at);
        if (!started_) {
            started_ = true;
            run_start_ = event.at;
            time_to_first_ = event.at - offered_at_;
        }
    }

    void TimedMode::on_tick(Millis now, const TypingModel& /*model*/) { now_ = std::max(now_, now); }

    Millis TimedMode::elapsed() const noexcept { return started_ ? now_ - run_start_ : Millis{0}; }

    bool TimedMode::is_finished() const { return started_ && elapsed() >= duration_; }

    ModeProgress TimedMode::progress() const {
        const Millis spent = elapsed();
        return TimedProgress{.elapsed = spent, .remaining = std::max(Millis{0}, duration_ - spent)};
    }

}  // namespace typeit::core
