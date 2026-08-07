#include "typeit/app/ingest/PlainText.h"

#include <array>
#include <cstddef>
#include <filesystem>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "typeit/app/ingest/MimeDetection.h"
#include "typeit/core/util/Result.h"

namespace typeit::app {
    namespace {

        /// Characters as bytes. The one direction that has to be spelled out;
        /// `as_text` is the other.
        [[nodiscard]] std::vector<std::byte> bytes_of(std::string_view text) {
            std::vector<std::byte> bytes;
            bytes.reserve(text.size());
            for (const char letter: text) {
                bytes.push_back(static_cast<std::byte>(static_cast<unsigned char>(letter)));
            }
            return bytes;
        }

    }  // namespace

    bool FileFetcher::can_handle(std::string_view locator) const {
        // Not "does this file exist": a path that does not is a `FileNotFound`
        // with a message naming it, which is far more useful than a locator no
        // fetcher will admit to.
        return !locator.empty() && locator != "-";
    }

    core::Result<FetchedContent> FileFetcher::fetch(std::string_view locator) const {
        const std::filesystem::path path{locator};

        // The same port Phase 6 read through, so a missing file and a directory
        // still come back as the different codes they always did.
        core::Result<std::string> contents = files_->read_text(path);
        if (!contents) {
            return std::unexpected{contents.error()};
        }

        FetchedContent fetched;
        fetched.bytes = bytes_of(*contents);
        fetched.detected_mime = detect_mime(fetched.bytes, locator);
        fetched.origin = std::string{locator};
        // The file's name without its extension, which is what Phase 6 titled
        // an imported file after.
        fetched.suggested_title = path.stem().string();
        return fetched;
    }

    core::Result<FetchedContent> MemoryFetcher::fetch(std::string_view locator) const {
        if (locator != origin_) {
            return core::fail(core::ErrorCode::InvalidArgument,
                              std::string{locator} + ": this fetcher holds " + origin_);
        }

        FetchedContent fetched;
        fetched.bytes = bytes_of(content_);
        // Detected from the content alone: a paste has no name to read an
        // extension off, and guessing one from the first line would be a guess
        // dressed up as a fact.
        fetched.detected_mime = detect_mime(fetched.bytes, {});
        fetched.origin = origin_;
        return fetched;
    }

    std::span<const std::string_view> PlainTextExtractor::mime_types() const {
        // Code and subtitles are here until TX-003 and TX-004 give them
        // extractors of their own; each of those issues takes its type off this
        // list, and the registry's no-two-claims rule will say so loudly if one
        // forgets. TX-002 took `text/markdown` off it.
        //
        // Passing them through unchanged is exactly what Phase 6 did. Leaving
        // them off would refuse a `.srt` file that imported yesterday, which is
        // a behaviour change dressed up as a refactor.
        static constexpr std::array<std::string_view, 4> kTypes{"text/plain", "text/x-code", "text/x-subrip",
                                                                "text/vtt"};
        return kTypes;
    }

    core::Result<ExtractedText> PlainTextExtractor::extract(const FetchedContent& content) const {
        // A passthrough, deliberately. Validating the UTF-8 is normalisation's
        // job and it reports the byte offset; doing it here as well would be a
        // second opinion with a worse message.
        ExtractedText extracted;
        extracted.text = std::string{as_text(content)};
        if (!content.suggested_title.empty()) {
            extracted.title = content.suggested_title;
        }
        return extracted;
    }

}  // namespace typeit::app
