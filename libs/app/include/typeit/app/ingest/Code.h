// Source code, as typing material (TX-003, TEXT_SOURCES §4).
//
// Typing code is what a large share of this application's likely users do all
// day, and it exercises symbols, brackets and indentation that prose never
// touches. That last one is the whole design constraint: **the bytes come out
// exactly as they went in**. A line indented with two tabs stays two tabs, a
// line indented with seven spaces stays seven spaces, and a trailing space
// stays unless somebody asked for it to go.
//
// This is not a parser and does not want to be. Sections come from a brace or
// indent heuristic that is right about ordinary code and approximately right
// about the rest; getting a section boundary slightly wrong costs somebody a
// bookmark in an odd place, and a parser per language costs a compiler
// front-end per language.
#ifndef TYPEIT_APP_INGEST_CODE_H
#define TYPEIT_APP_INGEST_CODE_H

#include <cstddef>
#include <span>
#include <string>
#include <string_view>

#include "typeit/app/ingest/Ingestion.h"
#include "typeit/core/util/Result.h"

namespace typeit::app {

    struct CodeOptions {
        /// Take the comments out.
        ///
        /// Off by default, and that default is the interesting one: comments
        /// are prose written by a programmer, they are a large fraction of what
        /// anybody types in a working day, and a file with them removed is not
        /// the file anybody actually works on.
        bool strip_comments = false;

        /// Keep the whitespace at the end of a line.
        ///
        /// On by default, because "exactly as it went in" is the promise, and
        /// because a trailing space is a real thing an editor will make
        /// somebody type. It is separable because a text somebody types for
        /// practice with `strict_spaces` on and trailing whitespace kept is a
        /// text with invisible characters they must guess at.
        bool keep_trailing_whitespace = true;

        /// A line longer than this is reported as poor typing material.
        ///
        /// Minified JavaScript is one line of forty thousand characters. It is
        /// technically importable and there is no way to type it, so the import
        /// says so rather than leaving somebody to discover it at the keyboard.
        std::size_t long_line = 400;
    };

    /// The language a filename implies, or empty. Lowercased, from the
    /// extension only — the content of a source file is exactly the thing this
    /// project has no parser for.
    [[nodiscard]] std::string language_of(std::string_view path);

    /// Code, kept as code.
    class CodeExtractor final : public ITextExtractor {
    public:
        explicit CodeExtractor(CodeOptions options = {}) : options_{options} {}

        [[nodiscard]] std::span<const std::string_view> mime_types() const override;

        [[nodiscard]] core::Result<ExtractedText> extract(const FetchedContent& content) const override;

    private:
        CodeOptions options_;
    };

}  // namespace typeit::app

#endif  // TYPEIT_APP_INGEST_CODE_H
