#include "typeit/app/services/TextLibraryService.h"

#include <algorithm>
#include <cstddef>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

#include "typeit/app/ingest/PlainText.h"
#include "typeit/app/ingest/Sections.h"
#include "typeit/app/records/TextLibrary.h"
#include "typeit/core/text/Difficulty.h"
#include "typeit/core/text/TextBuffer.h"
#include "typeit/core/text/TextNormalizer.h"
#include "typeit/core/text/Whitespace.h"
#include "typeit/core/util/Result.h"
#include "typeit/core/util/Sha256.h"
#include "typeit/core/util/Units.h"

namespace typeit::app {
    namespace {

        /// Long enough to tell two pastes apart in a list, short enough to fit
        /// on a line next to the rest of the row.
        constexpr std::size_t kGeneratedTitleGraphemes = 40;

        bool is_only_whitespace(std::string_view text) {
            return std::ranges::all_of(text, [](char byte) { return core::is_ascii_space(byte); });
        }

        /// The opening words, for content that arrived without a name. A
        /// library of things called "Untitled" is not a library.
        std::string title_from_content(const core::TextBuffer& buffer) {
            const std::size_t taken = std::min(buffer.size(), kGeneratedTitleGraphemes);
            std::string title = buffer.to_string(core::GraphemeIndex{0}, core::GraphemeIndex{taken});
            std::ranges::replace(title, '\n', ' ');
            while (!title.empty() && title.back() == ' ') {
                title.pop_back();
            }
            if (taken < buffer.size()) {
                title += "...";
            }
            return title;
        }

    }  // namespace

    core::Result<TextLibraryService::Prepared> TextLibraryService::prepare(const std::filesystem::path& path) {
        // Fetch, extract, then make typeable (TX-001, TX-007, ADR-013). The
        // stages are new; the behaviour is not — a missing file and a directory
        // still come back as the different codes the port has always given,
        // because the fetcher reads through that same port.
        const FileFetcher fetcher{*files_};
        core::Result<FetchedContent> fetched = fetcher.fetch(path.string());
        if (!fetched) {
            return std::unexpected{fetched.error()};
        }

        const ITextExtractor* const extractor = extractors_.find(fetched->detected_mime);
        if (extractor == nullptr) {
            // Named, because "cannot import this" without saying what it looked
            // like leaves somebody guessing at their own file.
            return core::fail(core::ErrorCode::FileUnreadable, "nothing here can read " + fetched->detected_mime);
        }

        core::Result<ExtractedText> extracted = extractor->extract(*fetched);
        if (!extracted) {
            return std::unexpected{extracted.error()};
        }

        // The extractor's normalisation when it has an opinion, the library's
        // otherwise. Code is the case that needs one: normalised as prose, its
        // indentation collapses to single spaces and the one thing the code
        // extractor promises is gone before anybody types a character.
        const core::NormalizeOptions normalization = extracted->normalization.value_or(normalization_);

        // The readiness pass deletes lines and merges others, so the
        // extractor's chapter boundaries go through the line map it returns.
        // Without that, dropping one running head moves every chapter marker in
        // the book up by a line (TX-005, TX-007).
        ReadinessResult typeable =
                make_typeable(extracted->text, extracted->readiness.value_or(readiness_), normalization);
        std::vector<SectionBoundary> boundaries = remapped(extracted->sections, extracted->text, typeable);
        // Only when it changed something. The copy exists to hold what the
        // running heads and page numbers were before they went, and keeping a
        // byte-identical second copy of every text would double the database to
        // answer a question nobody asks.
        std::optional<std::string> as_imported = typeable.text == extracted->text
                                                         ? std::nullopt
                                                         : std::optional<std::string>{std::move(extracted->text)};

        return Prepared{.text = std::move(typeable.text),
                        .suggested_title = std::move(fetched->suggested_title),
                        .normalization = normalization,
                        .extraction = Extraction{.boundaries = std::move(boundaries),
                                                 .warnings = std::move(extracted->warnings),
                                                 .mime = fetched->detected_mime,
                                                 .extractor = std::string{extractor->name()},
                                                 .author = std::move(extracted->author),
                                                 .readiness = std::move(typeable.report),
                                                 .as_imported = std::move(as_imported)}};
    }

    core::Result<ReadinessReport> TextLibraryService::inspect_file(const std::filesystem::path& path) {
        core::Result<Prepared> prepared = prepare(path);
        if (!prepared) {
            return std::unexpected{prepared.error()};
        }
        return std::move(prepared->extraction.readiness);
    }

    core::Result<ImportOutcome> TextLibraryService::import_file(const std::filesystem::path& path,
                                                                std::optional<std::string> title) {
        core::Result<Prepared> prepared = prepare(path);
        if (!prepared) {
            return std::unexpected{prepared.error()};
        }
        return store(
                std::move(prepared->text), TextSource::File, path.string(),
                title.has_value() ? std::move(title) : std::optional<std::string>{std::move(prepared->suggested_title)},
                prepared->normalization, std::move(prepared->extraction));
    }

    core::Result<DirectoryImport> TextLibraryService::import_directory(const std::filesystem::path& path) {
        if (!files_->is_directory(path)) {
            // Distinguished from "does not exist" by the port, which already
            // tells the two apart; saying "not a directory" about a path that
            // is not there at all sends somebody looking for the wrong mistake.
            return files_->exists(path) ? core::fail(core::ErrorCode::FileUnreadable,
                                                     path.string() + ": this is a file, not a folder")
                                        : core::fail(core::ErrorCode::FileNotFound, path.string() + ": no such folder");
        }

        core::Result<std::vector<std::filesystem::path>> children = files_->list(path);
        if (!children) {
            return std::unexpected{children.error()};
        }
        if (children->empty()) {
            return core::fail(core::ErrorCode::EmptyText, path.string() + ": there is nothing in this folder");
        }

        DirectoryImport summary;
        for (const std::filesystem::path& child: *children) {
            if (files_->is_directory(child)) {
                // Immediate children only. Recursing would import a source
                // tree's vendored dependencies from one keystroke.
                summary.skipped.push_back(child.filename().string() + ": a folder, and folders are not recursed into");
                continue;
            }
            core::Result<ImportOutcome> imported = import_file(child);
            if (imported) {
                summary.imported.push_back(std::move(*imported));
                continue;
            }
            // One bad file does not cost somebody the other hundred and
            // ninety-nine. The reason travels with the name, because a count of
            // failures tells them something is wrong without telling them what.
            summary.skipped.push_back(child.filename().string() + ": " + imported.error().context);
        }
        return summary;
    }

    namespace {

        /// The byte-order marks that mean "this is not UTF-8", and what to call
        /// them.
        ///
        /// Checked before validation so the message says `UTF-16LE` rather than
        /// "invalid UTF-8 at byte 0". The second is true and useless: it sends
        /// somebody hunting for one corrupt character when the whole file is in
        /// another encoding, which is a different fix entirely.
        ///
        /// UTF-32LE first, because its mark starts with UTF-16LE's.
        [[nodiscard]] std::string_view foreign_encoding(std::string_view content) {
            // Spelled with explicit lengths. A `const char*` literal stops at
            // its first NUL, so `"\x00\x00\xFE\xFF"` is the *empty* string and
            // `starts_with` on it is true of everything — which is how the
            // first draft of this rejected every plain UTF-8 file as UTF-32BE.
            constexpr std::string_view kUtf32Le{"\xFF\xFE\x00\x00", 4};
            constexpr std::string_view kUtf32Be{"\x00\x00\xFE\xFF", 4};
            constexpr std::string_view kUtf16Le{"\xFF\xFE", 2};
            constexpr std::string_view kUtf16Be{"\xFE\xFF", 2};

            if (content.starts_with(kUtf32Le)) {
                return "UTF-32LE";
            }
            if (content.starts_with(kUtf32Be)) {
                return "UTF-32BE";
            }
            if (content.starts_with(kUtf16Le)) {
                return "UTF-16LE";
            }
            if (content.starts_with(kUtf16Be)) {
                return "UTF-16BE";
            }
            return {};
        }

        /// A UTF-8 byte-order mark, gone.
        ///
        /// It is valid UTF-8, which is exactly the problem: it decodes to
        /// U+FEFF and becomes a grapheme at the head of the text that the
        /// typist has to type and cannot see. Stripped rather than rejected —
        /// a file saved by Notepad is a normal file, and refusing it would be
        /// refusing most of Windows.
        [[nodiscard]] std::string_view without_utf8_bom(std::string_view content) {
            constexpr std::string_view kBom{"\xEF\xBB\xBF", 3};
            return content.starts_with(kBom) ? content.substr(kBom.size()) : content;
        }

    }  // namespace

    core::Result<ImportOutcome> TextLibraryService::import_text(std::string content, TextSource source,
                                                                std::optional<std::string> origin,
                                                                std::optional<std::string> title) {
        // The readiness *steps* are off: a paste is what somebody meant to
        // type, and reflowing their poem or deciding their `[1]` was a footnote
        // would be editing it. The *report* is still produced, because they
        // deserve to know it has em dashes in it either way.
        ReadinessResult typeable = make_typeable(content, ReadinessOptions::none(), normalization_);
        return store(std::move(content), source, std::move(origin), std::move(title), normalization_,
                     Extraction{.boundaries = {},
                                .warnings = {},
                                .mime = std::nullopt,
                                .extractor = std::nullopt,
                                .author = std::nullopt,
                                .readiness = std::move(typeable.report),
                                .as_imported = std::nullopt});
    }

    core::Result<ImportOutcome> TextLibraryService::store(std::string content, TextSource source,
                                                          std::optional<std::string> origin,
                                                          std::optional<std::string> title,
                                                          const core::NormalizeOptions& normalization,
                                                          Extraction extraction) {
        std::vector<std::string> warnings = std::move(extraction.warnings);
        // Every path that reaches here has already produced one; carried
        // through so that a caller who never asked for the report still gets
        // it rather than having to inspect the file a second time.
        ReadinessReport readiness = std::move(extraction.readiness);
        if (content.size() > kMaxImportBytes) {
            // Naming the limit, because "too large" without a number is a
            // message that sends someone to the source code.
            return core::fail(core::ErrorCode::TextTooLarge,
                              origin.value_or("<text>") + ": " + std::to_string(content.size()) +
                                      " bytes exceeds the " + std::to_string(kMaxImportBytes) + " byte limit");
        }

        if (const std::string_view encoding = foreign_encoding(content); !encoding.empty()) {
            return core::fail(core::ErrorCode::InvalidUtf8, origin.value_or("<text>") + ": this file is " +
                                                                    std::string{encoding} +
                                                                    ", not UTF-8; convert it first");
        }

        // Normalising first: it is what validates the UTF-8, with the byte
        // offset, and everything downstream — the hash, the count, the score —
        // has to see the same bytes the typist will. The BOM goes before that,
        // so two copies of one text — one saved with a mark and one without —
        // hash the same and import once.
        const std::string_view body = without_utf8_bom(content);
        core::Result<std::string> normalized = core::normalize(body, normalization);
        if (!normalized) {
            return std::unexpected{normalized.error()};
        }

        if (normalized->empty() || is_only_whitespace(*normalized)) {
            // The empty-file case 1.0 reads past the end of (defect C1), now
            // an error with something to read.
            return core::fail(core::ErrorCode::EmptyText,
                              origin.value_or("<text>") + ": there is nothing here to type");
        }

        const std::string hash = core::sha256_hex(*normalized);

        // Deduplication is by content, not by path: the same file imported
        // twice is one text, and the same file changed is a new one.
        const core::Result<std::optional<TextItem>> found = library_->find_by_hash(hash);
        if (!found) {
            return std::unexpected{found.error()};
        }
        // Named rather than reached through two dereferences: `(*found)->id` is
        // the same access and clang-tidy cannot see the check through the
        // Result wrapping the optional.
        const std::optional<TextItem>& existing = found.value();
        if (existing.has_value()) {
            return ImportOutcome{.id = existing->id,
                                 .already_present = true,
                                 .warnings = std::move(warnings),
                                 .readiness = std::move(readiness)};
        }

        core::Result<core::TextBuffer> buffer = core::TextBuffer::from_utf8(*normalized);
        if (!buffer) {
            return std::unexpected{buffer.error()};
        }

        // Before `content` is moved out below. The boundaries index into it, so
        // reading it afterwards would be reading a moved-from string — and the
        // one case that matters, a text whose normalisation changed nothing, is
        // exactly the case where the move does not happen and the bug hides.
        std::vector<TextSection> sections = sections_for(extraction.boundaries, content, *buffer);

        TextItem item;
        item.title = title.has_value() && !title->empty() ? *std::move(title) : title_from_content(*buffer);
        item.source = source;
        item.origin = std::move(origin);
        item.content = *normalized;
        // Only when normalising changed something. Keeping a byte-identical
        // copy of every text would double the database to answer a question
        // nobody asks.
        // The extractor's own output when the readiness pass rewrote it,
        // otherwise the content as it arrived here — and nothing at all when
        // neither step changed a byte.
        if (extraction.as_imported.has_value()) {
            item.content_raw = std::move(extraction.as_imported);
        } else if (content != *normalized) {
            item.content_raw = std::move(content);
        }
        item.content_sha256 = hash;
        item.grapheme_count = buffer->size();
        item.word_count = buffer->word_count();
        item.difficulty = core::difficulty_score(*normalized);
        item.created_at = clock_->unix_now();
        // Mapped onto the normalised buffer rather than passed through: the
        // extractor's offsets are into bytes it emitted before any of this ran,
        // and handing those on unchanged would be a chapter marker pointing
        // wherever normalisation happened to leave it.
        item.sections = std::move(sections);
        item.author = std::move(extraction.author);
        item.mime = std::move(extraction.mime);
        item.extractor = std::move(extraction.extractor);

        const core::Result<core::TextId> id = library_->add(item);
        if (!id) {
            return std::unexpected{id.error()};
        }
        return ImportOutcome{.id = *id,
                             .already_present = false,
                             .warnings = std::move(warnings),
                             .readiness = std::move(readiness)};
    }

    core::Result<TextProgress> TextLibraryService::progress(core::TextId id) const {
        const core::Result<std::optional<TextItem>> text = library_->get(id);
        if (!text) {
            return std::unexpected{text.error()};
        }
        if (!text->has_value()) {
            return core::fail(core::ErrorCode::FileNotFound, "no text with id " + std::to_string(id.value));
        }

        const core::Result<std::optional<Bookmark>> mark = library_->bookmark(id);
        if (!mark) {
            return std::unexpected{mark.error()};
        }

        TextProgress out;
        out.total = (*text)->grapheme_count;
        // No bookmark is offset zero, not an error: not having started is the
        // normal state of most of a library.
        out.offset = mark->has_value() ? (*mark)->offset : core::GraphemeIndex{0};
        out.offset = core::GraphemeIndex{std::min(out.offset.value, out.total)};
        // An empty text counts as finished rather than dividing by zero. There
        // is nothing left to type either way, and 0/0 is not a percentage.
        out.finished = out.total == 0 || out.offset.value >= out.total;
        out.fraction = out.total == 0 ? 1.0 : static_cast<double>(out.offset.value) / static_cast<double>(out.total);
        return out;
    }

    core::Status TextLibraryService::advance(core::TextId id, std::size_t graphemes_completed) {
        const core::Result<TextProgress> current = progress(id);
        if (!current) {
            return std::unexpected{current.error()};
        }
        // Clamped: a caller that over-counts must not leave a bookmark pointing
        // past the last grapheme, which every reader of it would then have to
        // defend against.
        const std::size_t moved = std::min(current->offset.value + graphemes_completed, current->total);
        return bookmark(id, core::GraphemeIndex{moved});
    }

    core::Status TextLibraryService::reset_progress(core::TextId id) { return bookmark(id, core::GraphemeIndex{0}); }

    core::Status TextLibraryService::bookmark(core::TextId id, core::GraphemeIndex offset) {
        // Which chapter that offset is in, recorded alongside it (TX-006). Read
        // here rather than computed by whoever displays it: a section index
        // derived at read time is one that disagrees with the offset the moment
        // a text is re-imported, and this is written once per run.
        const core::Result<std::optional<TextItem>> text = library_->get(id);
        if (!text) {
            return std::unexpected{text.error()};
        }
        if (!text->has_value()) {
            return core::fail(core::ErrorCode::FileNotFound, "no text with id " + std::to_string(id.value));
        }

        Bookmark mark;
        mark.text_id = id;
        mark.offset = offset;
        mark.updated_at = clock_->unix_now();
        mark.section_idx = section_at((*text)->sections, offset);
        return library_->set_bookmark(mark);
    }

}  // namespace typeit::app
