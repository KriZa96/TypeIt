#include "typeit/app/ingest/Sections.h"

#include <algorithm>
#include <cstddef>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "typeit/app/ingest/Ingestion.h"
#include "typeit/app/records/TextLibrary.h"
#include "typeit/core/text/Grapheme.h"
#include "typeit/core/text/TextBuffer.h"
#include "typeit/core/util/Units.h"

namespace typeit::app {
    namespace {

        /// The grapheme each line of the normalised text starts at.
        ///
        /// Line zero starts at grapheme zero, and a text with no newline in it
        /// has exactly that one entry. A trailing newline does not open a line:
        /// a file ending in `\n` is not a file with an empty last line, and
        /// treating it as one would give the last section an end past the text.
        [[nodiscard]] std::vector<std::size_t> line_starts(const core::TextBuffer& text) {
            std::vector<std::size_t> starts{0};
            std::size_t at = 0;
            for (const core::Grapheme& grapheme: text.graphemes()) {
                ++at;
                if (grapheme.view() == "\n" && at < text.size()) {
                    starts.push_back(at);
                }
            }
            return starts;
        }

        /// Where line `line` starts, or the end of the text if there is no such
        /// line.
        ///
        /// Past the last line happens when normalisation dropped the tail — a
        /// document ending in a heading and nothing else — and the answer is an
        /// empty section rather than an index the buffer cannot be read at.
        [[nodiscard]] std::size_t start_of_line(std::span<const std::size_t> starts, std::size_t line,
                                                const core::TextBuffer& normalized) {
            if (line >= starts.size()) {
                return normalized.size();
            }
            // Unchecked after the comparison above, as elsewhere in this layer:
            // at() would throw, which nothing here does (ADR-009).
            // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-avoid-unchecked-container-access)
            return starts[line];
        }

        /// Which line of `extracted` byte `offset` falls on.
        [[nodiscard]] std::size_t line_of(std::string_view extracted, std::size_t offset) {
            return static_cast<std::size_t>(
                    std::ranges::count(extracted.substr(0, std::min(offset, extracted.size())), '\n'));
        }

        /// A boundary's title, or nothing.
        ///
        /// Empty and absent are the same thing said twice, and a section
        /// displayed as "" is a row of the library screen with a blank where a
        /// chapter name goes.
        [[nodiscard]] std::optional<std::string> title_of(const SectionBoundary& boundary) {
            if (boundary.title.empty()) {
                return std::nullopt;
            }
            return boundary.title;
        }

        /// Where line `line` starts in `text`, in bytes.
        [[nodiscard]] std::size_t byte_of_line(std::string_view text, std::size_t line) {
            std::size_t at = 0;
            for (std::size_t seen = 0; seen < line; ++seen) {
                const std::size_t newline = text.find('\n', at);
                if (newline == std::string_view::npos) {
                    return text.size();
                }
                at = newline + 1;
            }
            return at;
        }

    }  // namespace

    std::vector<SectionBoundary> remapped(std::span<const SectionBoundary> boundaries, std::string_view extracted,
                                          const ReadinessResult& typeable) {
        std::vector<SectionBoundary> moved;
        moved.reserve(boundaries.size());
        for (const SectionBoundary& boundary: boundaries) {
            const std::size_t line = typeable.line_from_source(line_of(extracted, boundary.start));
            // The length is not carried across: it described the old text, and
            // a stale length is worse than none. `sections_for` derives every
            // end from the next start anyway.
            moved.push_back(
                    SectionBoundary{.title = boundary.title, .start = byte_of_line(typeable.text, line), .length = 0});
        }
        return moved;
    }

    std::vector<TextSection> sections_for(std::span<const SectionBoundary> boundaries, std::string_view extracted,
                                          const core::TextBuffer& normalized) {
        const std::size_t total = normalized.size();
        std::vector<TextSection> sections;

        if (!boundaries.empty()) {
            const std::vector<std::size_t> starts = line_starts(normalized);
            for (const SectionBoundary& boundary: boundaries) {
                std::size_t start = start_of_line(starts, line_of(extracted, boundary.start), normalized);
                // Monotone by construction rather than by trust. Boundaries
                // arrive in order today; a section starting before the one
                // ahead of it would overlap, and an overlap is a bookmark in
                // two chapters at once.
                if (!sections.empty()) {
                    start = std::max(start, sections.back().start.value);
                }
                sections.push_back(TextSection{.idx = sections.size(),
                                               .title = title_of(boundary),
                                               .start = core::GraphemeIndex{start},
                                               .end = core::GraphemeIndex{total}});
            }
            // The text before the first boundary belongs to a section too, or a
            // bookmark landing in it would belong to none. A source file's
            // includes come before its first function; a document may open with
            // prose before its first heading.
            if (sections.front().start.value > 0) {
                sections.insert(sections.begin(), TextSection{.idx = 0,
                                                              .title = std::nullopt,
                                                              .start = core::GraphemeIndex{0},
                                                              .end = core::GraphemeIndex{total}});
                std::size_t at = 0;
                for (TextSection& section: sections) {
                    section.idx = at++;
                }
            }
        }

        if (sections.empty()) {
            // One section rather than none. "No sections" and "one section
            // covering everything" describe the same text, and only one of them
            // needs handling by everything downstream.
            sections.push_back(TextSection{.idx = 0,
                                           .title = std::nullopt,
                                           .start = core::GraphemeIndex{0},
                                           .end = core::GraphemeIndex{total}});
        }

        // Each section ends where the next begins. Written once here rather
        // than maintained as the sections are built, because a length kept
        // alongside a start is a second copy of the same fact. The last one
        // already ends at `total`, which is where the text does.
        for (auto section = sections.begin(); std::next(section) != sections.end(); ++section) {
            section->end = std::next(section)->start;
        }
        return sections;
    }

    std::size_t section_at(std::span<const TextSection> sections, core::GraphemeIndex offset) {
        // A linear scan over what is at most a few dozen chapters, and usually
        // one. A binary search would be the same answer computed less legibly.
        std::size_t found = 0;
        for (const TextSection& section: sections) {
            if (section.start > offset) {
                break;
            }
            found = section.idx;
        }
        return found;
    }

}  // namespace typeit::app
