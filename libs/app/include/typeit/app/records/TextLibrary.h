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
        std::size_t grapheme_count = 0;
        std::size_t word_count = 0;
        /// The 1–10 advisory score from TECHNICAL section 8.3.
        std::optional<double> difficulty;
        core::Millis created_at{0};
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
    };

}  // namespace typeit::app

#endif  // TYPEIT_APP_RECORDS_TEXTLIBRARY_H
