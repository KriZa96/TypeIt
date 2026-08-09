// The Phase 6 import path, behind the Phase 6A seams (TX-001).
//
// Nothing new happens here. The bytes a file holds, the bytes a pipe held, the
// bytes somebody pasted, and turning bytes into text — all of it existed, in
// one function. This is the same behaviour split at the three places the next
// format will need it split.
#ifndef TYPEIT_APP_INGEST_PLAINTEXT_H
#define TYPEIT_APP_INGEST_PLAINTEXT_H

#include <array>
#include <span>
#include <string>
#include <string_view>
#include <utility>

#include "typeit/app/ingest/Ingestion.h"
#include "typeit/app/ports/IFileSystem.h"
#include "typeit/core/util/Result.h"

namespace typeit::app {

    /// A path on disk.
    class FileFetcher final : public IContentFetcher {
    public:
        explicit FileFetcher(IFileSystem& files) : files_{&files} {}

        /// Anything that is not one of the other fetchers' shapes. A path is
        /// the default kind of locator, and deciding "is this a path" by
        /// asking the file system would refuse a path that does not exist yet
        /// — which is a `FileNotFound` with a useful message, not a locator
        /// nobody can handle.
        [[nodiscard]] bool can_handle(std::string_view locator) const override;

        [[nodiscard]] core::Result<FetchedContent> fetch(std::string_view locator) const override;

    private:
        IFileSystem* files_;
    };

    /// Bytes already in hand: a paste, or a pipe somebody has drained.
    ///
    /// It fetches nothing. It exists so that the pasted path and the file path
    /// are the *same* path from extraction onwards, rather than two that drift.
    class MemoryFetcher final : public IContentFetcher {
    public:
        MemoryFetcher(std::string content, std::string origin) :
            content_{std::move(content)}, origin_{std::move(origin)} {}

        /// Only the locator it was built for. A memory fetcher answering "yes"
        /// to everything would swallow every path in a registry that asked it
        /// first.
        [[nodiscard]] bool can_handle(std::string_view locator) const override { return locator == origin_; }

        [[nodiscard]] core::Result<FetchedContent> fetch(std::string_view locator) const override;

    private:
        std::string content_;
        std::string origin_;
    };

    /// Text that is already text.
    ///
    /// `text/plain` and nothing else, now that Markdown, code and subtitles
    /// have extractors of their own.
    class PlainTextExtractor final : public ITextExtractor {
    public:
        [[nodiscard]] std::string_view name() const override;

        [[nodiscard]] std::span<const std::string_view> mime_types() const override;

        [[nodiscard]] core::Result<ExtractedText> extract(const FetchedContent& content) const override;
    };

}  // namespace typeit::app

#endif  // TYPEIT_APP_INGEST_PLAINTEXT_H
