#include "typeit/core/session/KeystrokeLog.h"

#include <cassert>
#include <cstddef>

#include "typeit/core/session/Keystroke.h"
#include "typeit/core/util/Units.h"

namespace typeit::core {

    void KeystrokeLog::append(const Keystroke& event) {
        assert((events_.empty() || event.at >= events_.back().at) && "keystroke timestamps must not go backwards");
        events_.push_back(event);
    }

    void KeystrokeLog::reserve(std::size_t events) { events_.reserve(events); }

    Millis KeystrokeLog::duration() const noexcept {
        if (events_.empty()) {
            return Millis{0};
        }
        return events_.back().at - events_.front().at;
    }

}  // namespace typeit::core
