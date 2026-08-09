// The three stages a text passes through on its way in (TX-001, ADR-013).
//
//   bytes ──IContentFetcher──► FetchedContent ──ITextExtractor──► ExtractedText
//                                                                      │
//                                                          TextNormalizer, then the library
//
// Phase 6 did all three in one function. That worked while "a text" meant a
// UTF-8 file, and stops working the moment it can also mean an EPUB fetched
// over HTTP: fetching and extracting fail differently, are configured
// differently, and are useful in combinations nobody enumerates in advance.
// Adding EPUB should mean writing one extractor and touching nothing else.
//
// **This is a refactor and the Phase 6 tests are the proof.** Every one of them
// passes unchanged; if any needed editing, the seams would be in the wrong
// place.
#ifndef TYPEIT_APP_INGEST_INGESTION_H
#define TYPEIT_APP_INGEST_INGESTION_H

#include <cstddef>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "typeit/core/text/TextNormalizer.h"
#include "typeit/core/util/Result.h"
#include "typeit/core/util/Units.h"

namespace typeit::app {

    /// Where an extractor found a chapter, an article, a function.
    ///
    /// Offsets into the extracted text rather than copies of it: a book's
    /// chapters would otherwise double the memory of importing it.
    ///
    /// **Bytes, and before normalisation** — which is everything an extractor
    /// can honestly know, because normalisation has not run yet and will move
    /// every offset it does not delete. Import maps these onto the normalised
    /// text and hands out `TextSection`s measured in graphemes (TX-005); this
    /// type never leaves the pipeline, and nothing persists it.
    struct SectionBoundary {
        std::string title;
        std::size_t start = 0;
        std::size_t length = 0;
    };

    /// What a fetcher hands over: bytes, and everything it learned getting
    /// them.
    ///
    /// `bytes` rather than `std::string` because this stage has not decided the
    /// content is text yet — an EPUB is a ZIP, and calling it a string invites
    /// somebody to print it.
    struct FetchedContent {
        std::vector<std::byte> bytes;
        /// From headers, magic bytes, or the extension, in that order of trust.
        std::string detected_mime;
        /// The path or URL, recorded as the text's origin.
        std::string origin;
        /// A filename, an HTML `<title>`, EPUB metadata. The importer uses it
        /// only when nobody gave a better one.
        std::string suggested_title;
    };

    /// What an extractor makes of those bytes.
    struct ExtractedText {
        std::string text;
        /// Chapters, articles, functions. Empty is normal and means "one
        /// undivided text", which is what a plain file is.
        std::vector<SectionBoundary> sections;
        std::optional<std::string> title;
        std::optional<std::string> author;
        std::optional<std::string> language;

        /// Normalisation this text needs instead of the library's settings.
        ///
        /// Empty for prose, which is the whole point of having library-wide
        /// settings. Code is the case that forces it to exist: the default
        /// collapses runs of whitespace and expands tabs, which between them
        /// destroy every indented line in a source file — the one thing TX-003
        /// promises to preserve byte for byte. An extractor that knows its
        /// output is not prose says so here rather than hoping the user has
        /// configured the library the way its format needs.
        std::optional<core::NormalizeOptions> normalization;

        /// Things worth telling somebody about the text they just imported.
        ///
        /// Not errors: a file that mixes tabs and spaces still imports, because
        /// refusing it would be refusing a real file somebody wants to type.
        /// It is worth knowing before they wonder why the indentation will not
        /// match.
        std::vector<std::string> warnings;
    };

    /// Where bytes come from: a file, standard input, a paste, later a URL.
    class IContentFetcher {
    public:
        IContentFetcher() = default;
        virtual ~IContentFetcher();
        IContentFetcher(const IContentFetcher&) = delete;
        IContentFetcher& operator=(const IContentFetcher&) = delete;
        IContentFetcher(IContentFetcher&&) = delete;
        IContentFetcher& operator=(IContentFetcher&&) = delete;

        /// Whether this fetcher recognises the locator as its own. A path, a
        /// `-`, a `https://` URL: each fetcher answers for its own shape rather
        /// than a central table deciding for all of them.
        [[nodiscard]] virtual bool can_handle(std::string_view locator) const = 0;

        [[nodiscard]] virtual core::Result<FetchedContent> fetch(std::string_view locator) const = 0;
    };

    /// What turns bytes into text somebody can type.
    class ITextExtractor {
    public:
        ITextExtractor() = default;
        virtual ~ITextExtractor();
        ITextExtractor(const ITextExtractor&) = delete;
        ITextExtractor& operator=(const ITextExtractor&) = delete;
        ITextExtractor(ITextExtractor&&) = delete;
        ITextExtractor& operator=(ITextExtractor&&) = delete;

        /// The MIME types this extractor claims. A span of static strings, so
        /// the registry can index them without copying and an extractor cannot
        /// change its mind after registration.
        [[nodiscard]] virtual std::span<const std::string_view> mime_types() const = 0;

        [[nodiscard]] virtual core::Result<ExtractedText> extract(const FetchedContent& content) const = 0;
    };

    /// The bytes as text, when they are text.
    ///
    /// A free function rather than a method, because every extractor that deals
    /// in character data needs it and none of them should each write their own
    /// reinterpret_cast.
    [[nodiscard]] std::string_view as_text(const FetchedContent& content);

}  // namespace typeit::app

#endif  // TYPEIT_APP_INGEST_INGESTION_H
