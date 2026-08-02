// How many terminal columns a thing occupies.
//
// Everything the TUI positions — the cursor, the wrap points, the caret under
// a mistyped character — is counted in columns, and a wrong answer here is a
// cursor that drifts as you type. The tables come from the Unicode Character
// Database via scripts/generate-width-tables.py; this is only the lookup.
//
// Terminal emulators do not all agree about emoji width. That disagreement is
// real and is not resolved here: a per-terminal override lands with the
// capability detection in Phase 4 (TI-082).
#ifndef TYPEIT_CORE_TEXT_WIDTH_H
#define TYPEIT_CORE_TEXT_WIDTH_H

#include <cstdint>
#include <span>

#include "typeit/core/text/Grapheme.h"

namespace typeit::core {

    /// Columns for one code point: 0 for combining marks, joiners and controls,
    /// 2 for East Asian Wide/Fullwidth and emoji presentation, 1 otherwise.
    [[nodiscard]] std::uint8_t width_of(char32_t code_point) noexcept;

    /// Columns for a whole cluster.
    ///
    /// The base character decides, because the marks that follow it hang off it
    /// and take no column of their own. A variation selector overrides: U+FE0E
    /// asks for the text form at one column, U+FE0F for the emoji form at two —
    /// which is the difference between ❤ and ❤️.
    [[nodiscard]] std::uint8_t width_of_cluster(std::span<const char32_t> code_points) noexcept;

    /// Total columns of a run of clusters.
    [[nodiscard]] std::size_t total_width(std::span<const Grapheme> graphemes) noexcept;

}  // namespace typeit::core

#endif  // TYPEIT_CORE_TEXT_WIDTH_H
