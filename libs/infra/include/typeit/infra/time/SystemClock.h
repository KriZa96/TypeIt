// The real clock (TECHNICAL section 1.3).
//
// The only implementation of IClock the application ships; every test uses
// FakeClock instead, which is what removes every sleep from the suite.
#ifndef TYPEIT_INFRA_TIME_SYSTEMCLOCK_H
#define TYPEIT_INFRA_TIME_SYSTEMCLOCK_H

#include "typeit/core/util/IClock.h"
#include "typeit/core/util/Units.h"

namespace typeit::infra {

    /// Monotonic, not wall-clock: a run measured against the system clock gets
    /// longer or shorter when NTP corrects the machine, and a typist who gains
    /// 40 WPM at 02:00 on the last Sunday of October has not improved.
    ///
    /// The epoch is therefore arbitrary and only differences mean anything —
    /// which is exactly what IClock promises.
    class SystemClock final : public core::IClock {
    public:
        [[nodiscard]] core::Millis now() const override;
    };

    /// Milliseconds since the Unix epoch, for the things that genuinely are
    /// dates: when a session was run, when a text was imported. Kept apart from
    /// `now()` so that nobody reaches for a wall clock to measure a duration.
    [[nodiscard]] core::Millis unix_now();

}  // namespace typeit::infra

#endif  // TYPEIT_INFRA_TIME_SYSTEMCLOCK_H
