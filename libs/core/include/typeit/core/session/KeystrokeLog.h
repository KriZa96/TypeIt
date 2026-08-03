// The append-only event log a run is measured from (ADR-002).
//
// Append-only is the whole design, not an implementation detail: there is no
// erase, no rewind and no mutating accessor, so a metric can be recomputed
// later — with a better definition, or for a replay — and get the same answer.
// A backspace is an event that gets appended, never an event that removes one.
//
// Thread affinity: owned by the session, touched only from the event loop
// thread (ARCHITECTURE section 6.4).
#ifndef TYPEIT_CORE_SESSION_KEYSTROKELOG_H
#define TYPEIT_CORE_SESSION_KEYSTROKELOG_H

#include <cstddef>
#include <span>
#include <vector>

#include "typeit/core/session/Keystroke.h"
#include "typeit/core/util/Units.h"

namespace typeit::core {

    class KeystrokeLog {
    public:
        /// Precondition: `event.at` is not earlier than the last appended event.
        /// Time running backwards would make `duration()` a lie and every
        /// window-based metric undefined, so it is a contract violation rather
        /// than something to sort out later.
        void append(const Keystroke& event);

        /// Somewhere to put a run's worth of events up front. Purely a
        /// performance hint: it changes no observable value.
        void reserve(std::size_t events);

        [[nodiscard]] std::span<const Keystroke> events() const noexcept { return events_; }

        [[nodiscard]] std::size_t size() const noexcept { return events_.size(); }
        [[nodiscard]] bool empty() const noexcept { return events_.empty(); }

        /// Room currently allocated, in events. Exposed so the memory bound can
        /// be a test rather than an assumption.
        [[nodiscard]] std::size_t capacity() const noexcept { return events_.capacity(); }

        /// Last timestamp minus first. Zero for an empty log and for a log of
        /// one event — a run with nothing in it took no time, which is more
        /// useful than a negative number or a division by zero downstream.
        [[nodiscard]] Millis duration() const noexcept;

    private:
        std::vector<Keystroke> events_;
    };

}  // namespace typeit::core

#endif  // TYPEIT_CORE_SESSION_KEYSTROKELOG_H
