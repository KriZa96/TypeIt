#include "typeit/infra/time/SystemClock.h"

#include <chrono>

#include "typeit/core/util/Units.h"

namespace typeit::infra {

    core::Millis SystemClock::now() const {
        const auto since_epoch = std::chrono::steady_clock::now().time_since_epoch();
        return core::Millis{std::chrono::duration_cast<std::chrono::milliseconds>(since_epoch).count()};
    }

    core::Millis SystemClock::unix_now() const { return typeit::infra::unix_now(); }

    core::Millis unix_now() {
        const auto since_epoch = std::chrono::system_clock::now().time_since_epoch();
        return core::Millis{std::chrono::duration_cast<std::chrono::milliseconds>(since_epoch).count()};
    }

}  // namespace typeit::infra
