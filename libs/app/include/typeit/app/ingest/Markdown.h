// Markdown, as typing material (TX-002, TEXT_SOURCES §4).
//
// Extraction, not rendering. A CommonMark parser answers "what does this look
// like"; this answers "what did somebody write", which is a smaller question
// with a much smaller answer. The difference matters because the output is
// typed by a human: a URL is punctuation soup nobody wants to type, and the
// link text beside it is the sentence they actually wrote.
//
// The line scanner below is deliberately not a parser and gets nested
// structures approximately right rather than exactly right. A document that
// defeats it produces slightly odd typing material, not a wrong answer to
// anything — which is the trade CommonMark compliance would be bought with.
#ifndef TYPEIT_APP_INGEST_MARKDOWN_H
#define TYPEIT_APP_INGEST_MARKDOWN_H

#include <span>
#include <string_view>

#include "typeit/app/ingest/Ingestion.h"
#include "typeit/core/util/Result.h"

namespace typeit::app {

    struct MarkdownOptions {
        /// Hand back the source unchanged, markup and all.
        ///
        /// For somebody practising Markdown itself — the syntax is a fair share
        /// of what a technical writer types all day, and stripping it would
        /// remove exactly the characters they came to practise.
        bool preserve_markup = false;
    };

    /// Markdown with its markup taken off.
    ///
    /// Headings become sections and stay in the text; fenced code keeps every
    /// byte of its whitespace; link text survives and URLs do not.
    class MarkdownExtractor final : public ITextExtractor {
    public:
        explicit MarkdownExtractor(MarkdownOptions options = {}) : options_{options} {}

        [[nodiscard]] std::span<const std::string_view> mime_types() const override;

        [[nodiscard]] core::Result<ExtractedText> extract(const FetchedContent& content) const override;

    private:
        MarkdownOptions options_;
    };

}  // namespace typeit::app

#endif  // TYPEIT_APP_INGEST_MARKDOWN_H
