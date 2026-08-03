// The cursor and the per-position state of a run (TECHNICAL section 1.6).
//
// Time arrives as a parameter. The model reads no clock, touches no I/O, holds
// no UI type, and is single-threaded by contract (ARCHITECTURE section 6.4) —
// which is what lets a scripted headless run take exactly the same code path
// as a real one.
//
// It keeps two things: the states the screen paints, and the log everything is
// measured from. The states are a view of the log, not a second source of
// truth; only `type` and `backspace` change either.
#ifndef TYPEIT_CORE_SESSION_TYPINGMODEL_H
#define TYPEIT_CORE_SESSION_TYPINGMODEL_H

#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

#include "typeit/core/session/KeystrokeLog.h"
#include "typeit/core/text/Grapheme.h"
#include "typeit/core/text/TextBuffer.h"
#include "typeit/core/util/Units.h"

namespace typeit::core {

    /// The state of one target position (GAMEPLAY section 6).
    enum class GraphemeState : std::uint8_t {
        Pending,
        Correct,
        /// Wrong on the attempt that stands. Still wrong in the finished text.
        Incorrect,
        /// Wrong once, right now. Counts as a first-attempt error for accuracy
        /// and as correct for final correctness — keeping the two distinct is
        /// the whole reason this state exists.
        Corrected,
        /// Jumped over by a space before it was ever attempted.
        Missed,
    };

    class TypingModel {
    public:
        /// `target` must outlive the model: the session owns both, and copying a
        /// text per model would make a mode switch cost more than it earns.
        ///
        /// An empty target is legal and is immediately `at_end()`.
        explicit TypingModel(const TextBuffer& target);

        /// Applies one typed grapheme at time `at`.
        ///
        /// Past the end of the text this does nothing at all — no cursor
        /// movement, no logged event. Typing after the last grapheme is the
        /// user overrunning, not an attempt at a position that exists.
        ///
        /// A space typed where the text has something else jumps to the next
        /// word, marking everything skipped `Missed`. A space typed where the
        /// text has any separator — newline and CRLF included — matches it, so
        /// a line break is crossed with the space bar rather than with Enter.
        void type(const Grapheme& grapheme, Millis at);

        /// Retreats one position and returns it to `Pending`.
        ///
        /// At the start of the text this is a no-op that logs nothing: there is
        /// no position to delete. It is the boundary the legacy engine survives
        /// only by an unrelated guard (defect C7), so it is asserted behaviour
        /// here rather than an accident.
        ///
        /// The position does not forget it was once wrong. Retyping it
        /// correctly gives `Corrected`, never `Correct`.
        void backspace(Millis at);

        [[nodiscard]] GraphemeIndex cursor() const noexcept { return GraphemeIndex{cursor_}; }

        /// One state per target position, always — `states().size() == size()`.
        [[nodiscard]] std::span<const GraphemeState> states() const noexcept { return states_; }

        [[nodiscard]] const KeystrokeLog& log() const noexcept { return log_; }

        [[nodiscard]] std::size_t size() const noexcept { return target_.size(); }

        [[nodiscard]] bool at_end() const noexcept { return cursor_ >= target_.size(); }

    private:
        [[nodiscard]] bool matches(const Grapheme& grapheme, std::size_t position) const;
        void skip_to_next_separator();

        std::span<const Grapheme> target_;
        std::vector<GraphemeState> states_;
        /// Positions that have been wrong at least once, so a retype can be
        /// told from a first attempt after the state has been reset to
        /// `Pending`. Not derived from the states, because a backspace erases
        /// exactly that information from them.
        std::vector<bool> ever_wrong_;
        KeystrokeLog log_;
        std::size_t cursor_ = 0;
    };

}  // namespace typeit::core

#endif  // TYPEIT_CORE_SESSION_TYPINGMODEL_H
