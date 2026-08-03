#include "typeit/core/modes/ZenMode.h"

#include <algorithm>

#include "typeit/core/modes/IMode.h"
#include "typeit/core/session/Keystroke.h"
#include "typeit/core/session/TypingModel.h"
#include "typeit/core/util/Units.h"

namespace typeit::core {

    void ZenMode::on_start(Millis at, const TypingModel& /*model*/) {
        started_at_ = at;
        now_ = at;
    }

    void ZenMode::on_keystroke(const Keystroke& event, const TypingModel& /*model*/) {
        now_ = std::max(now_, event.at);
    }

    void ZenMode::on_tick(Millis now, const TypingModel& /*model*/) { now_ = std::max(now_, now); }

    ModeProgress ZenMode::progress() const { return OpenProgress{.elapsed = now_ - started_at_}; }

}  // namespace typeit::core
