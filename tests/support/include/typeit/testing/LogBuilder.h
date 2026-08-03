// Synthetic keystroke logs, written the way a run reads (TESTING section 3.4).
//
//     const KeystrokeLog log = LogBuilder{}.type("h", Millis{100})
//                                          .type("q", Millis{200})
//                                          .backspace(Millis{300})
//                                          .type("e", Millis{400})
//                                          .build();
//
// Every metric test is built on one of these, which is what makes them exact,
// instant and free of timing dependence: no clock, no sleeping, and the answer
// is known before the code runs because the log was constructed to have it.
//
// The builder assigns target indices the way the model does — type advances,
// backspace retreats — but deliberately does not go through TypingModel. A
// metric test should keep meaning the same thing when a typing rule changes.
#ifndef TYPEIT_TESTING_LOGBUILDER_H
#define TYPEIT_TESTING_LOGBUILDER_H

#include <cstddef>
#include <span>
#include <string_view>

#include "typeit/core/session/KeystrokeLog.h"
#include "typeit/core/util/Units.h"

namespace typeit::testing {

    class LogBuilder {
    public:
        /// Appends the first grapheme of `grapheme` at time `at`.
        LogBuilder& type(std::string_view grapheme, core::Millis at);

        LogBuilder& backspace(core::Millis at);

        [[nodiscard]] const core::KeystrokeLog& build() const noexcept { return log_; }

    private:
        core::KeystrokeLog log_;
        std::size_t cursor_ = 0;
    };

    /// Every grapheme of `text`, typed correctly, evenly spaced so that the
    /// gross WPM of the result is exactly `speed` on the GAMEPLAY section 4.1
    /// definition: `(graphemes / 5) / minutes`, elapsed measured from the first
    /// keystroke to the last.
    [[nodiscard]] core::KeystrokeLog perfect(std::string_view text, core::Wpm speed);

    /// `perfect`, with `errors` of the graphemes typed wrong and left wrong —
    /// spread through the text rather than bunched at the front, and never on a
    /// separator, so a word boundary is not what is being measured.
    ///
    /// Precondition: `errors` is no more than the number of non-separator
    /// graphemes in `text`.
    [[nodiscard]] core::KeystrokeLog with_errors(std::string_view text, std::size_t errors,
                                                 core::Wpm speed = core::Wpm{60.0});

    /// The same keystrokes and the same elapsed time as `perfect`, delivered in
    /// bursts with idle gaps between them.
    ///
    /// Identical totals are the point: a consistency metric that scores this
    /// the same as an even run is not measuring what it claims (TI-038).
    [[nodiscard]] core::KeystrokeLog bursty(std::string_view text, core::Wpm speed = core::Wpm{60.0});

    /// `perfect`, with each of `gaps` inserted as idle time, spread evenly
    /// through the run. The keystrokes and their order are unchanged; only the
    /// clock moves.
    [[nodiscard]] core::KeystrokeLog with_pauses(std::string_view text, std::span<const core::Millis> gaps,
                                                 core::Wpm speed = core::Wpm{60.0});

}  // namespace typeit::testing

#endif  // TYPEIT_TESTING_LOGBUILDER_H
