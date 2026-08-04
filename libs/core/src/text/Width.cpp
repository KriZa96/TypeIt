#include "typeit/core/text/Width.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <span>

#include "typeit/core/text/Grapheme.h"
#include "typeit/core/text/WidthTables.h"

namespace typeit::core {
    namespace {

        constexpr char32_t kTextPresentationSelector = 0xFE0E;  // VS15: render as text
        constexpr char32_t kEmojiPresentationSelector = 0xFE0F;  // VS16: render as emoji

        /// Binary search over the sorted, coalesced ranges the generator emits.
        bool contains(std::span<const tables::CodePointRange> ranges, char32_t code_point) noexcept {
            const auto after = std::ranges::upper_bound(ranges, code_point, {}, &tables::CodePointRange::first);
            if (after == ranges.begin()) {
                return false;
            }
            const tables::CodePointRange& candidate = *std::prev(after);
            return code_point <= candidate.last;
        }

    }  // namespace

    std::uint8_t width_of(char32_t code_point) noexcept {
        if (contains(tables::kZeroWidth, code_point)) {
            return 0;
        }
        if (contains(tables::kWide, code_point)) {
            return 2;
        }
        return 1;
    }

    std::uint8_t width_of_cluster(std::span<const char32_t> code_points) noexcept {
        // The segmenter never produces an empty cluster, so this is a guard
        // against a caller that does not exist rather than a case with a test.
        if (code_points.empty()) {
            return 0;
        }

        for (const char32_t code_point: code_points) {
            if (code_point == kEmojiPresentationSelector) {
                return 2;
            }
            if (code_point == kTextPresentationSelector) {
                return 1;
            }
        }

        return width_of(code_points.front());
    }

    std::size_t total_width(std::span<const Grapheme> graphemes) noexcept {
        std::size_t columns = 0;
        for (const Grapheme& grapheme: graphemes) {
            columns += grapheme.width;
        }
        return columns;
    }

}  // namespace typeit::core
