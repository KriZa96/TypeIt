// Type this text, all of it, and stop (GAMEPLAY section 2.3).
//
// The simplest termination there is — the cursor reaching the end — and the
// one the legacy engine got right by accident: `should_finish_game()` returns
// early on an empty text, which is the only reason it does not read past the
// end of an empty line. Here it is the stated behaviour.
#ifndef TYPEIT_CORE_MODES_QUOTEMODE_H
#define TYPEIT_CORE_MODES_QUOTEMODE_H

#include <cstddef>
#include <string_view>

#include "typeit/core/modes/IMode.h"
#include "typeit/core/session/Keystroke.h"
#include "typeit/core/session/TypingModel.h"
#include "typeit/core/util/Units.h"

namespace typeit::core {

    class QuoteMode final : public IMode {
    public:
        void on_start(Millis at, const TypingModel& model) override;
        void on_keystroke(const Keystroke& event, const TypingModel& model) override;
        void on_tick(Millis now, const TypingModel& model) override;

        /// True the moment the cursor reaches the end of the text — and true
        /// from the outset for an empty one, which is a finished run rather
        /// than a crash.
        [[nodiscard]] bool is_finished() const override { return finished_; }

        [[nodiscard]] ModeProgress progress() const override;
        [[nodiscard]] std::string_view id() const override { return "quote"; }

        /// Exactly 0 at the start, 1 at the end, and the fraction of the text
        /// behind the cursor in between. Zero-length text is complete.
        [[nodiscard]] double completion() const noexcept;

    private:
        void observe(const TypingModel& model);

        std::size_t position_ = 0;
        std::size_t total_ = 0;
        bool finished_ = false;
    };

}  // namespace typeit::core

#endif  // TYPEIT_CORE_MODES_QUOTEMODE_H
