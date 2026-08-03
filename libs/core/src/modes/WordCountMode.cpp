#include "typeit/core/modes/WordCountMode.h"

#include <cassert>
#include <cstddef>
#include <span>
#include <vector>

#include "typeit/core/modes/IMode.h"
#include "typeit/core/session/Keystroke.h"
#include "typeit/core/session/TypingModel.h"
#include "typeit/core/text/Grapheme.h"
#include "typeit/core/text/Whitespace.h"
#include "typeit/core/util/Units.h"

namespace typeit::core {
    namespace {

        /// How many words are behind the cursor at each position it can hold.
        ///
        /// A word ends at the first separator after it, so the count goes up on
        /// the separator itself rather than on the last letter. The end of the
        /// text closes whatever word is open there, which is what lets the last
        /// word of a text count without a trailing space.
        std::vector<std::size_t> committed_by_position(std::span<const Grapheme> text) {
            std::vector<std::size_t> committed(text.size() + 1, 0);
            std::size_t words = 0;
            bool inside_word = false;

            for (std::size_t position = 0; position < text.size(); ++position) {
                // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-avoid-unchecked-container-access) -- position < size
                const bool separator = is_word_separator(text[position]);
                if (separator && inside_word) {
                    ++words;
                    inside_word = false;
                } else if (!separator) {
                    inside_word = true;
                }
                // Reaching the position *after* this grapheme is what commits
                // the word it ended.
                // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-avoid-unchecked-container-access) -- position + 1 <= size
                committed[position + 1] = words;
            }

            if (inside_word) {
                committed.back() = words + 1;
            }
            return committed;
        }

    }  // namespace

    WordCountMode::WordCountMode(std::size_t words) : target_words_{words} {
        assert(words > 0 && "a run of no words is over before it begins");
    }

    void WordCountMode::on_start(Millis /*at*/, const TypingModel& model) { observe(model); }

    void WordCountMode::observe(const TypingModel& model) {
        if (committed_at_.empty()) {
            committed_at_ = committed_by_position(model.target());
        }
        // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-avoid-unchecked-container-access) -- cursor <= size
        committed_ = committed_at_[model.cursor().value];
        // Sticky: backspacing over the space that ended the last word does not
        // reopen a run the typist has already been shown the results of.
        finished_ = finished_ || committed_ >= target_words_;
    }

    void WordCountMode::on_keystroke(const Keystroke& /*event*/, const TypingModel& model) { observe(model); }

    void WordCountMode::on_tick(Millis /*now*/, const TypingModel& model) { observe(model); }

    ModeProgress WordCountMode::progress() const { return WordProgress{.done = committed_, .total = target_words_}; }

}  // namespace typeit::core
