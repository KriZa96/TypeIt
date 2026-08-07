// What these bytes are (TX-001, TEXT_SOURCES §4).
//
// Three sources of truth, in descending order of trust:
//
//   1. what the user said, in configuration — they are looking at the file
//   2. the magic bytes — the content cannot lie about itself
//   3. the extension — a hint somebody typed, and often wrong
//
// The order matters and is the whole point. A `.txt` file that is actually a
// ZIP is a real thing: a download named badly, an EPUB somebody renamed. Trust
// the extension over the content and the plain-text extractor is handed a ZIP
// and reports it as invalid UTF-8, which sends the user looking for a corrupt
// character in a file that is not text at all.
#ifndef TYPEIT_APP_INGEST_MIMEDETECTION_H
#define TYPEIT_APP_INGEST_MIMEDETECTION_H

#include <cstddef>
#include <optional>
#include <span>
#include <string>
#include <string_view>

namespace typeit::app {

    /// The type these bytes look like, or empty when nothing recognises them.
    ///
    /// Only formats this project can name are detected. A general-purpose
    /// sniffer would be a second `file(1)` to maintain, and every type it knew
    /// that no extractor handled would be a more confident way of failing.
    [[nodiscard]] std::string mime_from_magic(std::span<const std::byte> bytes);

    /// The type a name suggests, or empty. Lowercased, and the *last* extension
    /// only: `notes.txt.md` is markdown, not text.
    [[nodiscard]] std::string mime_from_extension(std::string_view path);

    /// The three sources, resolved.
    ///
    /// `override_mime` is the user's, and wins outright: they can see the file
    /// and this cannot. Magic bytes come next, then the extension, then
    /// `text/plain` — because a file with no extension and no signature is
    /// usually somebody's notes, and refusing it would refuse the commonest
    /// thing anybody imports.
    ///
    /// The override is an `optional` rather than a second `string_view` so that
    /// a caller cannot pass the two the wrong way round — `detect_mime(bytes,
    /// override, path)` compiles and quietly declares every file to be whatever
    /// the path happens to spell.
    [[nodiscard]] std::string detect_mime(std::span<const std::byte> bytes, std::string_view path,
                                          std::optional<std::string_view> override_mime = std::nullopt);

}  // namespace typeit::app

#endif  // TYPEIT_APP_INGEST_MIMEDETECTION_H
