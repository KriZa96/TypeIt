// A game mode is a strategy behind one interface (ADR-008).
//
// Timed, words, quote, zen, endless, race and drill differ in when a run ends,
// where its text comes from and what the HUD shows — not in how typing works.
// Keeping that behind an interface is what stops the session engine growing a
// `switch` on mode, which is the coupling the rebuild exists to remove.
//
// A mode observes; it does not type. The model owns the cursor and the log, and
// a mode is handed both to look at.
#ifndef TYPEIT_CORE_MODES_IMODE_H
#define TYPEIT_CORE_MODES_IMODE_H

#include <cstddef>
#include <string_view>
#include <variant>

#include "typeit/core/session/Keystroke.h"
#include "typeit/core/session/TypingModel.h"
#include "typeit/core/util/Units.h"

namespace typeit::core {

    /// Time left of a fixed duration.
    struct TimedProgress {
        Millis elapsed{0};
        Millis remaining{0};
    };

    /// Words committed out of a target count.
    struct WordProgress {
        std::size_t done = 0;
        std::size_t total = 0;
    };

    /// How far through a fixed text the cursor is.
    struct TextProgress {
        GraphemeIndex position{0};
        std::size_t total = 0;
    };

    /// A run with no end: there is nothing to be a fraction of, so only
    /// elapsed time is reported.
    struct OpenProgress {
        Millis elapsed{0};
    };

    /// Whatever the HUD has to draw. A variant rather than a struct of
    /// everything, so a mode cannot leave half the fields meaningless and the
    /// HUD cannot read a field this mode never fills in.
    using ModeProgress = std::variant<TimedProgress, WordProgress, TextProgress, OpenProgress>;

    class IMode {
    public:
        IMode() = default;
        virtual ~IMode() = default;
        IMode(const IMode&) = delete;
        IMode& operator=(const IMode&) = delete;
        IMode(IMode&&) = delete;
        IMode& operator=(IMode&&) = delete;

        /// The run begins. Called once, before any keystroke, and the only
        /// chance a mode gets to look at the text before anything happens to
        /// it — which a mode over a fixed text needs in order to know it is
        /// already finished before a key is ever pressed.
        virtual void on_start(Millis at, const TypingModel& model) = 0;

        /// One keystroke has just been applied to `model`.
        virtual void on_keystroke(const Keystroke& event, const TypingModel& model) = 0;

        /// Time has passed. Called whether or not anything was typed — a timed
        /// run has to end while the typist stares at the screen, and a race has
        /// to keep moving.
        virtual void on_tick(Millis now, const TypingModel& model) = 0;

        /// Once this is true it stays true. A mode that flickers back to
        /// unfinished would restart a run the user has already been shown the
        /// results of.
        [[nodiscard]] virtual bool is_finished() const = 0;

        [[nodiscard]] virtual ModeProgress progress() const = 0;

        /// The stable identifier this mode is registered and persisted under.
        [[nodiscard]] virtual std::string_view id() const = 0;
    };

}  // namespace typeit::core

#endif  // TYPEIT_CORE_MODES_IMODE_H
