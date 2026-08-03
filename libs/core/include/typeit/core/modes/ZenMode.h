// Type until you stop (GAMEPLAY section 2.4).
//
// No timer, no word count, no end of text: the run finishes when the user quits
// it, which is a decision made above this layer. Everything is still measured —
// the log does not care why a run ended — and progress reports elapsed time
// rather than a fraction of something there is no total for.
#ifndef TYPEIT_CORE_MODES_ZENMODE_H
#define TYPEIT_CORE_MODES_ZENMODE_H

#include <string_view>

#include "typeit/core/modes/IMode.h"
#include "typeit/core/session/Keystroke.h"
#include "typeit/core/session/TypingModel.h"
#include "typeit/core/util/Units.h"

namespace typeit::core {

    class ZenMode final : public IMode {
    public:
        void on_start(Millis at, const TypingModel& model) override;
        void on_keystroke(const Keystroke& event, const TypingModel& model) override;
        void on_tick(Millis now, const TypingModel& model) override;

        /// Never. A zen run ends because the user ended it.
        [[nodiscard]] bool is_finished() const override { return false; }

        [[nodiscard]] ModeProgress progress() const override;
        [[nodiscard]] std::string_view id() const override { return "zen"; }

    private:
        Millis started_at_{0};
        Millis now_{0};
    };

}  // namespace typeit::core

#endif  // TYPEIT_CORE_MODES_ZENMODE_H
