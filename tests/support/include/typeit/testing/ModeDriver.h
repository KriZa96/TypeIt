// The loop a session runs, small enough to live in a test.
//
// Start, then keystrokes and ticks in the order they happen. Every mode test
// from TI-044 on drives its mode this way, so the driver is shared rather than
// copied into each of them — a mode that behaves differently under a subtly
// different driver is exactly the bug these tests are for.
#ifndef TYPEIT_TESTING_MODEDRIVER_H
#define TYPEIT_TESTING_MODEDRIVER_H

#include <cstddef>
#include <string_view>

#include "typeit/core/modes/IMode.h"
#include "typeit/core/session/TypingModel.h"
#include "typeit/core/text/Grapheme.h"
#include "typeit/core/text/TextBuffer.h"
#include "typeit/core/util/Units.h"
#include "typeit/testing/TypingRun.h"

namespace typeit::testing {

    class ModeDriver {
    public:
        ModeDriver(std::string_view text, core::IMode& mode) : target_{text_of(text)}, model_{target_}, mode_{mode} {
            mode_.on_start(now_);
        }

        /// Keystrokes are 100 ms apart unless a test says otherwise. A refused
        /// keystroke logs nothing and is not reported to the mode, because it
        /// did not happen.
        void type(std::string_view text, core::Millis gap = core::Millis{100}) {
            const core::TextBuffer typed = text_of(text);
            for (const core::Grapheme& grapheme: typed.graphemes()) {
                now_ += gap;
                const std::size_t before = model_.log().size();
                model_.type(grapheme, now_);
                if (model_.log().size() > before) {
                    mode_.on_keystroke(model_.log().events().back(), model_);
                }
            }
        }

        void backspace(core::Millis gap = core::Millis{100}) {
            now_ += gap;
            const std::size_t before = model_.log().size();
            model_.backspace(now_);
            if (model_.log().size() > before) {
                mode_.on_keystroke(model_.log().events().back(), model_);
            }
        }

        void tick(std::size_t times = 1, core::Millis step = core::Millis{500}) {
            for (std::size_t i = 0; i < times; ++i) {
                now_ += step;
                mode_.on_tick(now_, model_);
            }
        }

        /// Time passing with nobody watching — no tick, no keystroke. What a
        /// frozen terminal looks like to a mode.
        void wait(core::Millis how_long) { now_ += how_long; }

        [[nodiscard]] core::Millis now() const noexcept { return now_; }
        [[nodiscard]] const core::TypingModel& model() const noexcept { return model_; }

    private:
        core::TextBuffer target_;
        core::TypingModel model_;
        core::IMode& mode_;
        core::Millis now_{0};
    };

}  // namespace typeit::testing

#endif  // TYPEIT_TESTING_MODEDRIVER_H
