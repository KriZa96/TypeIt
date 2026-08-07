#include "typeit/app/ingest/MimeDetection.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <cstddef>
#include <span>
#include <optional>
#include <string>
#include <string_view>

namespace typeit::app {
    namespace {

        /// A signature and what it means. Only formats this project can name:
        /// a general-purpose sniffer would be a second `file(1)` to maintain,
        /// and every type it knew that no extractor handled would be a more
        /// confident way of failing.
        struct Signature {
            std::string_view magic;
            std::string_view mime;
        };

        [[nodiscard]] std::span<const Signature> signatures() {
            static constexpr std::array<Signature, 6> kKnown{{
                    // ZIP, which is also EPUB and DOCX. Which of the three it
                    // is takes reading the archive, so that is the archive
                    // extractor's question and not this one's.
                    {.magic = std::string_view{"PK\x03\x04", 4}, .mime = "application/zip"},
                    {.magic = std::string_view{"PK\x05\x06", 4}, .mime = "application/zip"},
                    {.magic = std::string_view{"%PDF-", 5}, .mime = "application/pdf"},
                    {.magic = std::string_view{"\x1F\x8B", 2}, .mime = "application/gzip"},
                    // UTF-16 is text, and naming it is what lets the importer
                    // say "this is UTF-16, convert it" rather than "invalid
                    // UTF-8 at byte 0".
                    {.magic = std::string_view{"\xFF\xFE", 2}, .mime = "text/plain;charset=utf-16le"},
                    {.magic = std::string_view{"\xFE\xFF", 2}, .mime = "text/plain;charset=utf-16be"},
            }};
            return kKnown;
        }

        /// Extension to type. A table rather than a chain of comparisons, so
        /// adding a format is a line here and a registration there.
        [[nodiscard]] std::span<const Signature> extensions() {
            static constexpr std::array<Signature, 22> kKnown{{
                    {.magic = "txt", .mime = "text/plain"},
                    {.magic = "text", .mime = "text/plain"},
                    {.magic = "md", .mime = "text/markdown"},
                    {.magic = "markdown", .mime = "text/markdown"},
                    {.magic = "html", .mime = "text/html"},
                    {.magic = "htm", .mime = "text/html"},
                    {.magic = "epub", .mime = "application/epub+zip"},
                    {.magic = "docx", .mime = "application/vnd.openxmlformats-officedocument.wordprocessingml.document"},
                    {.magic = "pdf", .mime = "application/pdf"},
                    {.magic = "srt", .mime = "text/x-subrip"},
                    {.magic = "vtt", .mime = "text/vtt"},
                    // Source code. One type for all of it: what an extractor
                    // does with code — keep the whitespace, keep the symbols —
                    // does not depend on which language wrote it.
                    {.magic = "c", .mime = "text/x-code"},
                    {.magic = "h", .mime = "text/x-code"},
                    {.magic = "cpp", .mime = "text/x-code"},
                    {.magic = "hpp", .mime = "text/x-code"},
                    {.magic = "cc", .mime = "text/x-code"},
                    {.magic = "rs", .mime = "text/x-code"},
                    {.magic = "py", .mime = "text/x-code"},
                    {.magic = "js", .mime = "text/x-code"},
                    {.magic = "ts", .mime = "text/x-code"},
                    {.magic = "go", .mime = "text/x-code"},
                    {.magic = "java", .mime = "text/x-code"},
            }};
            return kKnown;
        }

        [[nodiscard]] bool starts_with(std::span<const std::byte> bytes, std::string_view magic) {
            if (bytes.size() < magic.size()) {
                return false;
            }
            // `first` is the bounds check: a signature longer than the file is
            // the ordinary case here — a zero-byte file, or a one-byte one —
            // and reading past the end for it would be undefined rather than
            // merely wrong. Both sides are projected to `unsigned char` because
            // a `std::byte` and a `char` are the same byte spelled twice, and
            // `char` may be signed.
            return std::ranges::equal(
                    bytes.first(magic.size()), magic, {}, [](std::byte byte) { return std::to_integer<unsigned char>(byte); },
                    [](char letter) { return static_cast<unsigned char>(letter); });
        }

    }  // namespace

    std::string mime_from_magic(std::span<const std::byte> bytes) {
        for (const Signature& known: signatures()) {
            if (starts_with(bytes, known.magic)) {
                return std::string{known.mime};
            }
        }
        return {};
    }

    std::string mime_from_extension(std::string_view path) {
        const std::size_t dot = path.find_last_of('.');
        // A dot in a directory name is not an extension, and a leading dot is a
        // hidden file rather than a nameless one with an extension.
        const std::size_t slash = path.find_last_of("/\\");
        if (dot == std::string_view::npos || (slash != std::string_view::npos && dot < slash) ||
            dot + 1 >= path.size() || dot == slash + 1) {
            return {};
        }

        std::string extension;
        for (const char letter: path.substr(dot + 1)) {
            extension += static_cast<char>(std::tolower(static_cast<unsigned char>(letter)));
        }

        const auto found = std::ranges::find(extensions(), extension, &Signature::magic);
        return found == extensions().end() ? std::string{} : std::string{found->mime};
    }

    std::string detect_mime(std::span<const std::byte> bytes, std::string_view path,
                            std::optional<std::string_view> override_mime) {
        if (override_mime && !override_mime->empty()) {
            // The user is looking at the file and this is not.
            return std::string{*override_mime};
        }
        if (std::string magic = mime_from_magic(bytes); !magic.empty()) {
            // Content over extension. A `.txt` that is really a ZIP is a real
            // thing — a download named badly, an EPUB somebody renamed — and
            // believing the name hands a ZIP to the plain-text extractor, which
            // reports invalid UTF-8 and sends the user hunting for a corrupt
            // character in a file that is not text at all.
            return magic;
        }
        if (std::string named = mime_from_extension(path); !named.empty()) {
            return named;
        }
        // No signature and no extension is usually somebody's notes. Refusing
        // it would refuse the commonest thing anybody imports.
        return "text/plain";
    }

}  // namespace typeit::app
