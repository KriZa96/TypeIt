// A TypingModel with the text it points at, plus a clock that ticks by itself.
//
// Exists so a test can say what was typed instead of managing a TextBuffer's
// lifetime, segmenting its own input and inventing timestamps — and so the
// model and the rules tests exercise the same harness rather than two subtly
// different ones.
#ifndef TYPEIT_TESTING_TYPINGRUN_H
#define TYPEIT_TESTING_TYPINGRUN_H

#include <cassert>
#include <cstddef>
#include <string>
#include <string_view>
#include <utility>

#include "typeit/core/session/TypingModel.h"
#include "typeit/core/session/TypingRules.h"
#include "typeit/core/text/Grapheme.h"
#include "typeit/core/text/TextBuffer.h"
#include "typeit/core/util/Result.h"
#include "typeit/core/util/Units.h"

namespace typeit::testing {

    inline core::TextBuffer text_of(std::string_view text) {
        core::Result<core::TextBuffer> buffer = core::TextBuffer::from_utf8(text);
        assert(buffer.has_value() && "a test fixture must be valid UTF-8");
        return std::move(*buffer);
    }

    class TypingRun {
    public:
        explicit TypingRun(std::string_view text, core::TypingRules rules = {}) :
            target_{text_of(text)}, model_{target_, rules} {}

        /// Types `text` grapheme by grapheme — so `type("čšž")` is three
        /// keystrokes, not seven bytes.
        TypingRun& type(std::string_view text) {
            const core::TextBuffer typed = text_of(text);
            for (const core::Grapheme& grapheme: typed.graphemes()) {
                model_.type(grapheme, tick());
            }
            return *this;
        }

        TypingRun& backspace(std::size_t times = 1) {
            for (std::size_t i = 0; i < times; ++i) {
                model_.backspace(tick());
            }
            return *this;
        }

        [[nodiscard]] const core::TypingModel& model() const { return model_; }
        [[nodiscard]] std::size_t cursor() const { return model_.cursor().value; }
        [[nodiscard]] std::size_t events() const { return model_.log().size(); }

        /// One character per position, so an expectation reads like the screen:
        /// `.` pending, `C` correct, `x` incorrect, `c` corrected, `M` missed.
        [[nodiscard]] std::string states() const {
            std::string rendered;
            for (const core::GraphemeState state: model_.states()) {
                switch (state) {
                    case core::GraphemeState::Pending:
                        rendered += '.';
                        break;
                    case core::GraphemeState::Correct:
                        rendered += 'C';
                        break;
                    case core::GraphemeState::Incorrect:
                        rendered += 'x';
                        break;
                    case core::GraphemeState::Corrected:
                        rendered += 'c';
                        break;
                    case core::GraphemeState::Missed:
                        rendered += 'M';
                        break;
                }
            }
            return rendered;
        }

    private:
        core::Millis tick() {
            now_ += core::Millis{100};
            return now_;
        }

        core::TextBuffer target_;
        core::TypingModel model_;
        core::Millis now_{0};
    };

}  // namespace typeit::testing

#endif  // TYPEIT_TESTING_TYPINGRUN_H
