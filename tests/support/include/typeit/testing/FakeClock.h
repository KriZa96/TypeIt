// A clock that only moves when a test says so.
//
// This lives in the test support library rather than in core because it is not
// domain code, and rather than in each test file because every later phase
// needs it: services, modes, the pacer, and the race ramp are all functions of
// elapsed time.
#ifndef TYPEIT_TESTING_FAKECLOCK_H
#define TYPEIT_TESTING_FAKECLOCK_H

#include "typeit/core/util/IClock.h"
#include "typeit/core/util/Units.h"

namespace typeit::testing {

    class FakeClock final : public core::IClock, public core::IWallClock {
    public:
        explicit FakeClock(core::Millis start = core::Millis{0}) : now_{start} {}

        [[nodiscard]] core::Millis now() const override { return now_; }

        /// The same reading. A fake clock has one hand: a test that needed the
        /// monotonic and the wall clock to disagree would say so by setting
        /// them apart, and no test has needed that yet.
        [[nodiscard]] core::Millis unix_now() const override { return now_; }

        /// Jump to an absolute reading.
        void set(core::Millis value) { now_ = value; }

        /// Move by a delta. Negative deltas are allowed: a test for what happens
        /// when a clock goes backwards needs to be able to make one go backwards.
        void advance(core::Millis delta) { now_ += delta; }

    private:
        core::Millis now_;
    };

}  // namespace typeit::testing

#endif  // TYPEIT_TESTING_FAKECLOCK_H
