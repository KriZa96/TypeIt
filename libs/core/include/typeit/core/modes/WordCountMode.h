// A run of a fixed number of words (GAMEPLAY section 2.2).
//
// A word is committed when the typist leaves it — on the space or newline that
// follows it, or on reaching the end of the text — not when its last letter is
// typed. Ending on a letter would finish the run mid-keystroke, before the
// typist has confirmed the word is the one they meant.
#ifndef TYPEIT_CORE_MODES_WORDCOUNTMODE_H
#define TYPEIT_CORE_MODES_WORDCOUNTMODE_H

#include <cstddef>
#include <string_view>
#include <vector>

#include "typeit/core/modes/IMode.h"
#include "typeit/core/session/Keystroke.h"
#include "typeit/core/session/TypingModel.h"
#include "typeit/core/util/Units.h"

namespace typeit::core {

    class WordCountMode final : public IMode {
    public:
        /// Precondition: at least one word. Zero words is not a short run, it
        /// is a run that was over before it began, and the config layer rejects
        /// it with a message.
        explicit WordCountMode(std::size_t words);

        void on_start(Millis at) override;
        void on_keystroke(const Keystroke& event, const TypingModel& model) override;
        void on_tick(Millis now, const TypingModel& model) override;

        [[nodiscard]] bool is_finished() const override { return finished_; }
        [[nodiscard]] ModeProgress progress() const override;
        [[nodiscard]] std::string_view id() const override { return "words"; }

        [[nodiscard]] std::size_t target_words() const noexcept { return target_words_; }
        [[nodiscard]] std::size_t committed_words() const noexcept { return committed_; }

    private:
        void observe(const TypingModel& model);

        std::size_t target_words_;
        std::size_t committed_ = 0;
        bool finished_ = false;
        /// Words committed by the time the cursor reaches each position, built
        /// once from the text. A word skipped over still counts: the typist
        /// left it behind, badly, and the run is that many words long either
        /// way.
        std::vector<std::size_t> committed_at_;
    };

}  // namespace typeit::core

#endif  // TYPEIT_CORE_MODES_WORDCOUNTMODE_H
