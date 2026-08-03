#include "typeit/core/session/TypingModel.h"

#include <cstddef>
#include <cstdint>
#include <vector>

#include "typeit/core/session/Keystroke.h"
#include "typeit/core/session/TypingRules.h"
#include "typeit/core/text/Grapheme.h"
#include "typeit/core/text/TextBuffer.h"
#include "typeit/core/text/Whitespace.h"
#include "typeit/core/util/Units.h"

namespace typeit::core {

    TypingModel::TypingModel(const TextBuffer& target, TypingRules rules) :
        target_{target.graphemes()}, rules_{rules}, states_(target.size(), GraphemeState::Pending),
        ever_wrong_(target.size(), false) {}

    bool TypingModel::target_is_separator(std::size_t position) const {
        // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-avoid-unchecked-container-access) -- callers check
        return position < target_.size() && is_word_separator(target_[position]);
    }

    bool TypingModel::current_word_has_an_error() const {
        // Back to the separator that started the word, or to the beginning.
        for (std::size_t position = cursor_; position > 0; --position) {
            const std::size_t previous = position - 1;
            if (target_is_separator(previous)) {
                return false;
            }
            // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-avoid-unchecked-container-access) -- previous < cursor_
            if (states_[previous] == GraphemeState::Incorrect) {
                return true;
            }
        }
        return false;
    }

    bool TypingModel::refuses(const Grapheme& grapheme) const {
        switch (rules_.stop_on_error) {
            case StopOnError::Off:
                return false;
            case StopOnError::Letter:
                // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-avoid-unchecked-container-access) -- cursor_ > 0
                return cursor_ > 0 && states_[cursor_ - 1] == GraphemeState::Incorrect;
            case StopOnError::Word:
                // Only leaving the word is refused. Inside it the typist is free
                // to keep going and fix the error on the way back.
                return (is_word_separator(grapheme) || target_is_separator(cursor_)) && current_word_has_an_error();
        }
        return false;
    }

    bool TypingModel::matches(const Grapheme& grapheme, std::size_t position) const {
        // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-avoid-unchecked-container-access) -- callers check
        const Grapheme& expected = target_[position];
        if (grapheme == expected) {
            return true;
        }
        // Any separator answers any other. The text's line breaks are crossed
        // with the space bar, which is what 1.0 did and what a typist expects;
        // requiring Enter at a newline would make a pasted document unusable.
        return is_word_separator(grapheme) && is_word_separator(expected);
    }

    void TypingModel::skip_to_next_separator() {
        while (cursor_ < target_.size()) {
            // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-avoid-unchecked-container-access) -- bounded above
            if (is_word_separator(target_[cursor_])) {
                return;
            }
            // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-avoid-unchecked-container-access) -- bounded above
            states_[cursor_] = GraphemeState::Missed;
            // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-avoid-unchecked-container-access) -- bounded above
            ever_wrong_[cursor_] = true;
            ++cursor_;
        }
    }

    void TypingModel::type(const Grapheme& grapheme, Millis at) {
        if (at_end() || refuses(grapheme)) {
            return;
        }

        // strict_spaces off: a space the typist did not type is absorbed rather
        // than charged. The position is credited and the keystroke goes to what
        // follows it, which is what makes running two words together forgivable.
        if (!rules_.strict_spaces && target_is_separator(cursor_) && !is_word_separator(grapheme)) {
            // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-avoid-unchecked-container-access) -- cursor_ < size
            states_[cursor_] = GraphemeState::Correct;
            ++cursor_;
            if (at_end()) {
                return;
            }
        }

        // A space where the text has a letter ends the word early. Everything
        // stepped over was never attempted, which is `Missed` rather than
        // wrong: the typist did not get it wrong, they did not type it.
        // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-avoid-unchecked-container-access) -- at_end checked
        if (rules_.space_advances_word && is_word_separator(grapheme) && !is_word_separator(target_[cursor_])) {
            skip_to_next_separator();
            if (at_end()) {
                // The last word, with no separator after it. The space resolved
                // against no position, so it is logged against the end.
                log_.append(Keystroke{.at = at,
                                      .target = static_cast<std::uint32_t>(cursor_),
                                      .kind = KeystrokeKind::Character,
                                      .typed = grapheme});
                return;
            }
        }

        const std::size_t position = cursor_;
        log_.append(Keystroke{.at = at,
                              .target = static_cast<std::uint32_t>(position),
                              .kind = KeystrokeKind::Character,
                              .typed = grapheme});

        if (matches(grapheme, position)) {
            // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-avoid-unchecked-container-access) -- position < size
            states_[position] = ever_wrong_[position] ? GraphemeState::Corrected : GraphemeState::Correct;
        } else {
            // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-avoid-unchecked-container-access) -- position < size
            states_[position] = GraphemeState::Incorrect;
            // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-avoid-unchecked-container-access) -- position < size
            ever_wrong_[position] = true;
        }
        ++cursor_;
    }

    void TypingModel::backspace(Millis at) {
        if (cursor_ == 0 || !rules_.allow_backspace || rules_.confidence_mode == ConfidenceMode::Max) {
            return;
        }
        // confidence_mode on: a finished word stays finished. The position
        // behind the cursor being a separator is exactly the step that would
        // re-open the word before it.
        if (rules_.confidence_mode == ConfidenceMode::On && target_is_separator(cursor_ - 1)) {
            return;
        }

        --cursor_;
        // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-avoid-unchecked-container-access) -- cursor_ < size
        states_[cursor_] = GraphemeState::Pending;
        log_.append(Keystroke{.at = at,
                              .target = static_cast<std::uint32_t>(cursor_),
                              .kind = KeystrokeKind::Backspace,
                              .typed = {}});
    }

}  // namespace typeit::core
