#include "typeit/core/modes/QuoteMode.h"

#include "typeit/core/modes/IMode.h"
#include "typeit/core/session/Keystroke.h"
#include "typeit/core/session/TypingModel.h"
#include "typeit/core/util/Units.h"

namespace typeit::core {

    void QuoteMode::observe(const TypingModel& model) {
        position_ = model.cursor().value;
        total_ = model.size();
        // Sticky, like every mode: backspacing off the last grapheme does not
        // unfinish a run whose results are already on screen.
        finished_ = finished_ || model.at_end();
    }

    void QuoteMode::on_start(Millis /*at*/, const TypingModel& model) { observe(model); }

    void QuoteMode::on_keystroke(const Keystroke& /*event*/, const TypingModel& model) { observe(model); }

    void QuoteMode::on_tick(Millis /*now*/, const TypingModel& model) { observe(model); }

    double QuoteMode::completion() const noexcept {
        if (total_ == 0) {
            return 1.0;
        }
        return static_cast<double>(position_) / static_cast<double>(total_);
    }

    ModeProgress QuoteMode::progress() const {
        return TextProgress{.position = GraphemeIndex{position_}, .total = total_};
    }

}  // namespace typeit::core
