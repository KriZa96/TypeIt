// The domain's text type (TECHNICAL section 1.4).
//
// A run of grapheme clusters in one contiguous allocation, indexed in
// clusters rather than bytes. Everything downstream — the typing model, the
// metrics, the wrapper — counts in these, which is what makes a two-byte č one
// character to type instead of two.
//
// Immutable once built. Wrapping is deliberately not here: it is a pure
// function of (text, columns) in TI-031, so a terminal resize is answered by
// calling it again rather than by rebuilding anything.
#ifndef TYPEIT_CORE_TEXT_TEXTBUFFER_H
#define TYPEIT_CORE_TEXT_TEXTBUFFER_H

#include <cstddef>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "typeit/core/text/Grapheme.h"
#include "typeit/core/util/Result.h"
#include "typeit/core/util/Units.h"

namespace typeit::core {

    class TextBuffer {
    public:
        /// Decodes and segments `text`. Fails with `ErrorCode::InvalidUtf8`,
        /// naming the byte, rather than substituting anything.
        ///
        /// An empty text is a valid buffer, not an error: a session with nothing
        /// to type is a caller's problem to reject, with a better message than
        /// this layer could give.
        [[nodiscard]] static Result<TextBuffer> from_utf8(std::string_view text);

        [[nodiscard]] std::size_t size() const noexcept { return graphemes_.size(); }
        [[nodiscard]] bool empty() const noexcept { return graphemes_.empty(); }

        /// Precondition: `index < size()`. Use `at_checked` where the index came
        /// from outside this layer.
        [[nodiscard]] const Grapheme& at(GraphemeIndex index) const;

        [[nodiscard]] Result<Grapheme> at_checked(GraphemeIndex index) const;

        [[nodiscard]] std::span<const Grapheme> graphemes() const noexcept { return graphemes_; }

        /// The bytes of `[from, to)`. Precondition: `from <= to <= size()`.
        [[nodiscard]] std::string to_string(GraphemeIndex from, GraphemeIndex to) const;

        [[nodiscard]] std::string to_string() const { return to_string(GraphemeIndex{0}, GraphemeIndex{size()}); }

        /// Whitespace-delimited runs, matching what 1.0 counted so that a WPM
        /// figure means the same thing across the rebuild.
        ///
        /// A non-breaking space does not separate words — that is what it is for —
        /// and text without spaces at all, CJK included, is one word. Words are
        /// the wrong unit for those scripts; the speed metrics use five-character
        /// words instead (GAMEPLAY section 4).
        [[nodiscard]] std::size_t word_count() const noexcept { return word_count_; }

        /// Terminal columns the whole text occupies.
        [[nodiscard]] std::size_t display_width() const noexcept { return display_width_; }

        /// Clusters that did not fit in `Grapheme::kMaxBytes` and lost their tail.
        [[nodiscard]] std::size_t truncated() const noexcept { return truncated_; }

    private:
        TextBuffer(std::vector<Grapheme> graphemes, std::size_t truncated);

        std::vector<Grapheme> graphemes_;
        std::size_t word_count_ = 0;
        std::size_t display_width_ = 0;
        std::size_t truncated_ = 0;
    };

}  // namespace typeit::core

#endif  // TYPEIT_CORE_TEXT_TEXTBUFFER_H
