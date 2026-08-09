// An imported text, as it is stored and as it is listed
// (TECHNICAL section 5, TEXT_SOURCES).
#ifndef TYPEIT_APP_RECORDS_TEXTLIBRARY_H
#define TYPEIT_APP_RECORDS_TEXTLIBRARY_H

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "typeit/core/util/Units.h"

namespace typeit::app {

    /// Where a text came from. A closed set, because the database says so: the
    /// `source` column has a CHECK constraint naming exactly these.
    enum class TextSource : std::uint8_t {
        Builtin,
        File,
        Paste,
        Stdin,
    };

    [[nodiscard]] std::string_view to_string(TextSource source);
    [[nodiscard]] std::optional<TextSource> text_source_from(std::string_view name);

    /// One named run of a text: a chapter, an article, a function (TX-005).
    ///
    /// Graphemes, and after normalisation — the same unit a bookmark is
    /// measured in, so "which chapter is this offset in" is a comparison rather
    /// than a conversion. An extractor's `SectionBoundary` is bytes into its own
    /// output and is mapped onto this at import; keeping the two apart is what
    /// stops a stale byte offset being read as a grapheme one.
    ///
    /// The sections of a text are contiguous, non-overlapping, and cover it
    /// end to end, because a bookmark measured in offsets has to land inside a
    /// section wherever it lands. A text nobody found any structure in has one
    /// section covering all of it, rather than none.
    struct TextSection {
        /// Position in the text's own list, from zero. What "Chapter 4 of 31"
        /// counts, and what a bookmark records.
        std::size_t idx = 0;
        /// Absent where the format gave no name — the prose before a document's
        /// first heading is a section and is not called anything.
        std::optional<std::string> title;
        core::GraphemeIndex start{0};
        /// Exclusive, and equal to the next section's `start`.
        core::GraphemeIndex end{0};

        friend bool operator==(const TextSection&, const TextSection&) = default;
    };

    /// A text with its content. What an import produces and what a run reads.
    struct TextItem {
        core::TextId id{0};
        std::string title;
        TextSource source = TextSource::Paste;
        /// The path, URL or description this came from. Absent for a paste.
        std::optional<std::string> origin;
        /// Normalised: what the typist actually sees (TEXT_SOURCES section 8).
        std::string content;
        /// As imported, when normalisation changed anything worth keeping.
        std::optional<std::string> content_raw;
        /// Of the normalised content. Unique — importing the same file twice is
        /// one text, not two.
        std::string content_sha256;
        std::optional<std::string> language;
        /// From the format's own metadata, where it has any: EPUB and Markdown
        /// front matter do, a text file does not.
        std::optional<std::string> author;
        /// What the content was detected as, and which extractor read it
        /// (TX-006). Both recorded for the first time somebody reports that a
        /// file imported wrongly: the answer to "which of eight extractors
        /// produced this" is otherwise a guess from the filename.
        std::optional<std::string> mime;
        std::optional<std::string> extractor;
        std::size_t grapheme_count = 0;
        std::size_t word_count = 0;
        /// The 1–10 advisory score from TECHNICAL section 8.3.
        std::optional<double> difficulty;
        core::Millis created_at{0};
        /// Always at least one, covering the whole text (TX-005).
        std::vector<TextSection> sections;
    };

    /// A text as the library screen lists it: everything except the content,
    /// which may be a megabyte and which nobody is reading yet.
    struct TextSummary {
        core::TextId id{0};
        std::string title;
        TextSource source = TextSource::Paste;
        std::optional<std::string> origin;
        std::size_t grapheme_count = 0;
        std::size_t word_count = 0;
        std::optional<double> difficulty;
        core::Millis created_at{0};
        std::vector<std::string> tags;
    };

    struct TextFilter {
        /// A text must carry every tag named here, not any of them: tags are
        /// how a library is narrowed.
        std::vector<std::string> tags;
        /// Matched against the title, case-insensitively.
        std::optional<std::string> search;
        std::size_t limit = 0;
    };

    /// How far through a long text somebody got. One per text: reading two
    /// bookmarks into one book is a question with no good answer.
    struct Bookmark {
        core::TextId text_id{0};
        core::GraphemeIndex offset{0};
        core::Millis updated_at{0};
        /// Which section `offset` is in (TX-006). Not nullable, because every
        /// text has at least one section covering all of it — "no section"
        /// describes nothing, and an optional here would be a case every reader
        /// defends against and none can produce.
        std::size_t section_idx = 0;
    };

}  // namespace typeit::app

#endif  // TYPEIT_APP_RECORDS_TEXTLIBRARY_H
