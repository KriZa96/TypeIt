#include "typeit/core/text/Wrapper.h"

#include <cassert>
#include <cstddef>
#include <limits>
#include <span>
#include <string_view>

#include "typeit/core/text/Grapheme.h"
#include "typeit/core/util/Units.h"

namespace typeit::core {
    namespace {

        constexpr std::size_t kNoSpace = std::numeric_limits<std::size_t>::max();

        bool is_line_break(const Grapheme& grapheme) {
            const std::string_view view = grapheme.view();
            return view == "\n" || view == "\r\n" || view == "\r";
        }

        bool is_space(const Grapheme& grapheme) { return grapheme.view() == " "; }

        std::size_t width_of(std::span<const Grapheme> run) {
            std::size_t columns = 0;
            for (const Grapheme& grapheme: run) {
                columns += grapheme.width;
            }
            return columns;
        }

    }  // namespace

    LineBreaks wrap(std::span<const Grapheme> text, std::size_t columns) {
        assert(columns > 0 && "wrap needs at least one column to put anything in");

        LineBreaks breaks;
        if (text.empty()) {
            return breaks;
        }

        breaks.starts.push_back(GraphemeIndex{0});

        std::size_t line_start = 0;
        std::size_t last_space = kNoSpace;
        std::size_t width = 0;

        for (std::size_t i = 0; i < text.size();) {
            // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-avoid-unchecked-container-access) -- i < size()
            const Grapheme& grapheme = text[i];

            if (is_line_break(grapheme)) {
                // The break character belongs to the line it ends, so that
                // concatenating the lines gives the text back.
                ++i;
                if (i < text.size()) {
                    breaks.starts.push_back(GraphemeIndex{i});
                }
                line_start = i;
                last_space = kNoSpace;
                width = 0;
                continue;
            }

            if (width + grapheme.width > columns) {
                if (width == 0) {
                    // Nothing on the line yet and it still does not fit: a single
                    // grapheme wider than the whole line. It overflows here rather
                    // than disappearing.
                    width = grapheme.width;
                    ++i;
                    continue;
                }

                // Break after the last space that fits, or mid-word when the word
                // is longer than the line.
                const bool have_space = last_space != kNoSpace && last_space >= line_start;
                const std::size_t next_start = have_space ? last_space + 1 : i;

                breaks.starts.push_back(GraphemeIndex{next_start});
                line_start = next_start;
                last_space = kNoSpace;
                width = width_of(text.subspan(next_start, i - next_start + 1));
                ++i;
                continue;
            }

            width += grapheme.width;
            if (is_space(grapheme)) {
                last_space = i;
            }
            ++i;
        }

        return breaks;
    }

}  // namespace typeit::core
