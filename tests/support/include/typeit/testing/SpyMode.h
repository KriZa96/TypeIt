// An IMode that records what was called on it, and finishes when told to.
//
// Every mode test needs a driver that plays a script of keystrokes and ticks
// through a mode; this is the mode that lets the driver itself be tested, and
// the one the session tests will use when there is nothing real to run.
#ifndef TYPEIT_TESTING_SPYMODE_H
#define TYPEIT_TESTING_SPYMODE_H

#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

#include "typeit/core/modes/IMode.h"
#include "typeit/core/session/Keystroke.h"
#include "typeit/core/session/TypingModel.h"
#include "typeit/core/util/Units.h"

namespace typeit::testing {

    class SpyMode final : public core::IMode {
    public:
        explicit SpyMode(std::string_view id = "spy", std::size_t finish_after_ticks = 0) :
            id_{id}, finish_after_ticks_{finish_after_ticks} {}

        void on_start(core::Millis at, const core::TypingModel& /*model*/) override {
            calls.emplace_back("on_start");
            started_at = at;
        }

        void on_keystroke(const core::Keystroke& event, const core::TypingModel& model) override {
            calls.emplace_back("on_keystroke");
            ++keystrokes;
            last_cursor = model.cursor();
            last_event_at = event.at;
        }

        void on_tick(core::Millis now, const core::TypingModel& /*model*/) override {
            calls.emplace_back("on_tick");
            ++ticks;
            last_tick = now;
            if (finish_after_ticks_ > 0 && ticks >= finish_after_ticks_) {
                finished_ = true;
            }
        }

        [[nodiscard]] bool is_finished() const override { return finished_; }

        [[nodiscard]] core::ModeProgress progress() const override {
            return core::OpenProgress{.elapsed = last_tick - started_at};
        }

        [[nodiscard]] std::string_view id() const override { return id_; }

        void finish() { finished_ = true; }

        std::vector<std::string> calls;
        std::size_t keystrokes = 0;
        std::size_t ticks = 0;
        core::Millis started_at{0};
        core::Millis last_tick{0};
        core::Millis last_event_at{0};
        core::GraphemeIndex last_cursor{0};

    private:
        std::string id_;
        std::size_t finish_after_ticks_;
        bool finished_ = false;
    };

}  // namespace typeit::testing

#endif  // TYPEIT_TESTING_SPYMODE_H
