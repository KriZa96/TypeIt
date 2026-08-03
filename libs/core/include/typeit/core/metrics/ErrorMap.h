// What was typed where something else belonged (TECHNICAL section 5,
// `error_pair`).
//
// Feeds the heatmap in Phase 5 and the drill generator in Phase 8: "you type m
// when you mean n" is a practisable fact in a way that "your accuracy is 94%"
// is not.
#ifndef TYPEIT_CORE_METRICS_ERRORMAP_H
#define TYPEIT_CORE_METRICS_ERRORMAP_H

#include <cstddef>
#include <map>
#include <string>
#include <utility>

#include "typeit/core/session/KeystrokeLog.h"
#include "typeit/core/text/TextBuffer.h"

namespace typeit::core {

    struct ErrorMap {
        /// `(expected, typed) → count`. One entry per position that was got
        /// wrong on its first attempt: retyping the same mistake at the same
        /// place is the same mistake, not a second one.
        std::map<std::pair<std::string, std::string>, std::size_t> substitutions;

        /// Graphemes the text asked for that were never typed at all, having
        /// been skipped over on the way to a later position. Counted only
        /// behind the furthest point reached — the part of a text a typist
        /// never got to is unfinished, not omitted.
        std::map<std::string, std::size_t> omissions;

        /// Graphemes typed where the text had no position left for them.
        std::map<std::string, std::size_t> insertions;

        [[nodiscard]] bool empty() const noexcept {
            return substitutions.empty() && omissions.empty() && insertions.empty();
        }
    };

    /// A corrected mistake still appears: you made it, even though you fixed
    /// it, and the point of this map is to know what to practise.
    [[nodiscard]] ErrorMap error_map(const KeystrokeLog& log, const TextBuffer& target);

}  // namespace typeit::core

#endif  // TYPEIT_CORE_METRICS_ERRORMAP_H
