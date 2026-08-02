#include "typeit/core/text/Segmenter.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string_view>
#include <utility>
#include <vector>

#include "typeit/core/text/Grapheme.h"
#include "typeit/core/text/Utf8.h"
#include "typeit/core/text/Width.h"
#include "typeit/core/util/Result.h"

namespace typeit::core {
    namespace {

        constexpr char32_t kZeroWidthJoiner = 0x200D;
        constexpr char32_t kCarriageReturn = 0x000D;
        constexpr char32_t kLineFeed = 0x000A;
        constexpr char32_t kRegionalIndicatorFirst = 0x1F1E6;
        constexpr char32_t kRegionalIndicatorLast = 0x1F1FF;

        using Range = std::pair<char32_t, char32_t>;

        /// The combining marks this project joins, as inclusive ranges. This is the
        /// common blocks rather than every Mn/Mc/Me in Unicode — the scope boundary is
        /// documented in the header, and TI-029 generates the width tables from real
        /// Unicode data files if that stops being enough.
        constexpr std::array<Range, 17> kCombiningMarks{{
                {0x0300, 0x036F},  // combining diacritical marks
                {0x0483, 0x0489},  // Cyrillic
                {0x0591, 0x05BD},  // Hebrew points
                {0x05BF, 0x05BF},
                {0x05C1, 0x05C2},
                {0x05C4, 0x05C5},
                {0x05C7, 0x05C7},
                {0x0610, 0x061A},  // Arabic
                {0x064B, 0x065F},
                {0x0670, 0x0670},
                {0x06D6, 0x06DC},
                {0x06DF, 0x06E4},
                {0x0900, 0x0903},  // Devanagari
                {0x093A, 0x094F},
                {0x1AB0, 0x1AFF},  // combining diacritical marks extended
                {0x1DC0, 0x1DFF},  // combining diacritical marks supplement
                {0x20D0, 0x20F0},  // combining marks for symbols, including U+20E3 keycap
        }};

        /// Variation selectors: U+FE0F is what turns ❤ into ❤️.
        constexpr std::array<Range, 3> kVariationSelectors{{
                {0xFE00, 0xFE0F},
                {0xFE20, 0xFE2F},  // combining half marks
                {0xE0100, 0xE01EF},
        }};

        constexpr bool in_any(std::span<const Range> ranges, char32_t code_point) {
            return std::ranges::any_of(ranges, [code_point](const Range& range) {
                return code_point >= range.first && code_point <= range.second;
            });
        }

        constexpr bool is_combining(char32_t code_point) {
            return in_any(kCombiningMarks, code_point) || in_any(kVariationSelectors, code_point);
        }

        constexpr bool is_regional_indicator(char32_t code_point) {
            return code_point >= kRegionalIndicatorFirst && code_point <= kRegionalIndicatorLast;
        }

        /// The break rules, in one place. `regional_indicator_is_open` is true
        /// when the previous code point was the first of a flag pair and is
        /// still waiting for its second.
        constexpr bool joins_previous(char32_t previous, char32_t code_point, bool regional_indicator_is_open) {
            if (is_combining(code_point)) {
                return true;
            }
            // A zero-width joiner binds on both sides: it joins to what
            // precedes it and pulls in what follows.
            if (previous == kZeroWidthJoiner || code_point == kZeroWidthJoiner) {
                return true;
            }
            if (previous == kCarriageReturn && code_point == kLineFeed) {
                return true;
            }
            return is_regional_indicator(code_point) && regional_indicator_is_open;
        }

        /// One cluster being built. Bytes are appended whole code points at a time, so
        /// a truncated cluster is never cut mid-sequence.
        class ClusterBuilder {
        public:
            void reset() {
                grapheme_ = Grapheme{.bytes = {}, .length = 0, .width = 1};
                code_points_.clear();
                lost_bytes_ = false;
            }

            [[nodiscard]] bool empty() const { return grapheme_.length == 0; }

            void append(std::string_view bytes, char32_t code_point) {
                // The code point counts towards the width even when its bytes
                // did not fit: a truncated cluster still occupies the columns
                // its base character asked for.
                code_points_.push_back(code_point);

                if (grapheme_.length + bytes.size() > Grapheme::kMaxBytes) {
                    lost_bytes_ = true;
                    return;
                }
                for (const char byte: bytes) {
                    grapheme_.bytes.at(grapheme_.length) = byte;
                    ++grapheme_.length;
                }
            }

            /// Finished: the width is only knowable once every code point in
            /// the cluster has been seen, because a variation selector at the
            /// end changes the answer.
            [[nodiscard]] Grapheme grapheme() const {
                Grapheme finished = grapheme_;
                finished.width = width_of_cluster(code_points_);
                return finished;
            }

            [[nodiscard]] bool lost_bytes() const { return lost_bytes_; }

        private:
            Grapheme grapheme_{.bytes = {}, .length = 0, .width = 1};
            std::vector<char32_t> code_points_;
            bool lost_bytes_ = false;
        };

    }  // namespace

    Result<Segmentation> segment(std::string_view text) {
        Segmentation result;
        // One cluster per code point is the worst case and the common one.
        result.graphemes.reserve(text.size());

        ClusterBuilder cluster;
        const auto flush = [&result, &cluster] {
            result.graphemes.push_back(cluster.grapheme());
            if (cluster.lost_bytes()) {
                ++result.truncated;
            }
        };

        char32_t previous = 0;
        bool have_previous = false;
        // Regional indicators pair up: the second one joins, the third starts a
        // new cluster. Without this a flag sequence would swallow the whole line.
        bool regional_indicator_is_open = false;

        std::size_t offset = 0;
        while (offset < text.size()) {
            const Result<DecodedCodePoint> decoded = decode_one(text, offset);
            if (!decoded) {
                return std::unexpected{decoded.error()};
            }
            const char32_t code_point = decoded->code_point;
            const std::string_view bytes = text.substr(offset, decoded->length);

            const bool joins = have_previous && joins_previous(previous, code_point, regional_indicator_is_open);

            if (!joins && !cluster.empty()) {
                flush();
                cluster.reset();
            }

            cluster.append(bytes, code_point);

            if (is_regional_indicator(code_point)) {
                // Open on the first of a pair, closed by the second so a third
                // starts afresh.
                regional_indicator_is_open = !joins;
            } else {
                regional_indicator_is_open = false;
            }

            previous = code_point;
            have_previous = true;
            offset += decoded->length;
        }

        if (!cluster.empty()) {
            flush();
        }

        result.graphemes.shrink_to_fit();
        return result;
    }

}  // namespace typeit::core
