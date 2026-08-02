// The time port (TECHNICAL section 1.3).
//
// Every duration in the domain is measured through this interface, which is
// what lets the test suite have no sleeps in it: today TimerTest sleeps for a
// second and asserts the string "9s", a race against the scheduler that is
// slow when it passes and mystifying when it does not. A FakeClock makes the
// same assertions exact and instant.
#ifndef TYPEIT_CORE_UTIL_ICLOCK_H
#define TYPEIT_CORE_UTIL_ICLOCK_H

#include "typeit/core/util/Units.h"

namespace typeit::core {

    class IClock {
    public:
        IClock() = default;
        IClock(const IClock&) = delete;
        IClock(IClock&&) = delete;
        IClock& operator=(const IClock&) = delete;
        IClock& operator=(IClock&&) = delete;
        virtual ~IClock() = default;

        /// Monotonic. The epoch is unspecified, so only differences are meaningful.
        [[nodiscard]] virtual Millis now() const = 0;
    };

}  // namespace typeit::core

#endif  // TYPEIT_CORE_UTIL_ICLOCK_H
