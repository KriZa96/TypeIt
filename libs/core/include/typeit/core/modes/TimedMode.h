// A run of a fixed length (GAMEPLAY section 2.1).
//
// The clock starts on the first keystroke, not on entering the screen. 1.0
// starts it when a session flag flips, which charges the typist for their
// reaction time, for reading the text and for however long the menu took —
// time that is then divided into their WPM.
//
// The waiting is not thrown away: it is reported separately as
// `time_to_first_keystroke()`, which is a measure of reaction and belongs
// nowhere near a speed.
#ifndef TYPEIT_CORE_MODES_TIMEDMODE_H
#define TYPEIT_CORE_MODES_TIMEDMODE_H

#include <string_view>

#include "typeit/core/modes/IMode.h"
#include "typeit/core/session/Keystroke.h"
#include "typeit/core/session/TypingModel.h"
#include "typeit/core/util/Units.h"

namespace typeit::core {

    class TimedMode final : public IMode {
    public:
        /// The range TECHNICAL section 6 validates `default_duration_s` against.
        /// Enforced there, where a bad value is a config error with a message;
        /// asserted here, where it would be a bug.
        static constexpr Millis kMinDuration{1'000};
        static constexpr Millis kMaxDuration{3'600'000};

        explicit TimedMode(Millis duration);

        void on_start(Millis at, const TypingModel& model) override;
        void on_keystroke(const Keystroke& event, const TypingModel& model) override;
        void on_tick(Millis now, const TypingModel& model) override;

        /// Never before the first keystroke, however long the typist stares at
        /// the screen.
        [[nodiscard]] bool is_finished() const override;

        /// Elapsed is unclamped — the run really did last that long — while
        /// remaining floors at zero, because a negative countdown is not a
        /// thing the HUD should have to think about.
        [[nodiscard]] ModeProgress progress() const override;

        [[nodiscard]] std::string_view id() const override { return "timed"; }

        [[nodiscard]] Millis duration() const noexcept { return duration_; }

        /// From the run being offered to the first key actually pressed. Zero
        /// until that happens.
        [[nodiscard]] Millis time_to_first_keystroke() const noexcept { return time_to_first_; }

        [[nodiscard]] bool has_started() const noexcept { return started_; }

    private:
        [[nodiscard]] Millis elapsed() const noexcept;

        Millis duration_;
        Millis offered_at_{0};
        Millis run_start_{0};
        Millis now_{0};
        Millis time_to_first_{0};
        bool started_ = false;
    };

}  // namespace typeit::core

#endif  // TYPEIT_CORE_MODES_TIMEDMODE_H
