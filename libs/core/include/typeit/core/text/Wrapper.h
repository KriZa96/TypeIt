// Line wrapping as a pure function of (text, columns).
//
// Deliberately not state baked into the text: 1.0 computes its lines once in
// Text's constructor, which is why the layout cannot survive a terminal resize
// (review section 3.3). Here a resize is answered by calling wrap() again with
// the new width, and nothing else in the session changes.
#ifndef TYPEIT_CORE_TEXT_WRAPPER_H
#define TYPEIT_CORE_TEXT_WRAPPER_H

#include <cstddef>
#include <span>
#include <vector>

#include "typeit/core/text/Grapheme.h"
#include "typeit/core/util/Units.h"

namespace typeit::core {

    /// Where each line begins. Line `i` is `[starts[i], starts[i + 1])`, and the
    /// last runs to the end of the text, so every grapheme belongs to exactly one
    /// line and concatenating the lines reproduces the input.
    struct LineBreaks {
        std::vector<GraphemeIndex> starts;

        [[nodiscard]] std::size_t size() const noexcept { return starts.size(); }
        [[nodiscard]] bool empty() const noexcept { return starts.empty(); }
    };

    /// Greedy wrapping at the last space that fits, measured in display columns.
    ///
    /// A word longer than the line is hard-broken at the column limit. A newline
    /// forces a break. The spaces at a break stay on the line they ended, so they
    /// are not carried to the start of the next one — which means a line's width
    /// counting its trailing spaces may reach `columns`, while its visible content
    /// never exceeds it.
    ///
    /// Precondition: `columns > 0`.
    ///
    /// The one exception to the width bound is a single grapheme wider than the
    /// whole line — a wide CJK character at `columns == 1`. It gets a line of its
    /// own and overflows it, because there is nowhere else to put it.
    [[nodiscard]] LineBreaks wrap(std::span<const Grapheme> text, std::size_t columns);

}  // namespace typeit::core

#endif  // TYPEIT_CORE_TEXT_WRAPPER_H
