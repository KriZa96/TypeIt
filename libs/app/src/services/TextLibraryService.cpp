#include "typeit/app/services/TextLibraryService.h"

#include <algorithm>
#include <cstddef>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

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

    core::Result<ImportOutcome> TextLibraryService::import_file(const std::filesystem::path& path,
                                                                std::optional<std::string> title) {
        // A missing file and a directory come back as different codes from the
        // port, and both are passed through rather than flattened into "could
        // not import": the person who typed the path needs to know which.
        core::Result<std::string> contents = files_->read_text(path);
        if (!contents) {
            return std::unexpected{contents.error()};
        }

        return import_text(std::move(*contents), TextSource::File, path.string(),
                           title.has_value() ? std::move(title) : std::optional<std::string>{path.stem().string()});
    }

    core::Result<ImportOutcome> TextLibraryService::import_text(std::string content, TextSource source,
                                                                std::optional<std::string> origin,
                                                                std::optional<std::string> title) {
        if (content.size() > kMaxImportBytes) {
            // Naming the limit, because "too large" without a number is a
            // message that sends someone to the source code.
            return core::fail(core::ErrorCode::TextTooLarge,
                              origin.value_or("<text>") + ": " + std::to_string(content.size()) +
                                      " bytes exceeds the " + std::to_string(kMaxImportBytes) + " byte limit");
        }

        // Normalising first: it is what validates the UTF-8, with the byte
        // offset, and everything downstream — the hash, the count, the score —
        // has to see the same bytes the typist will.
        core::Result<std::string> normalized = core::normalize(content, normalization_);
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
            return ImportOutcome{.id = existing->id, .already_present = true};
        }

        core::Result<core::TextBuffer> buffer = core::TextBuffer::from_utf8(*normalized);
        if (!buffer) {
            return std::unexpected{buffer.error()};
        }

        TextItem item;
        item.title = title.has_value() && !title->empty() ? *std::move(title) : title_from_content(*buffer);
        item.source = source;
        item.origin = std::move(origin);
        item.content = *normalized;
        // Only when normalising changed something. Keeping a byte-identical
        // copy of every text would double the database to answer a question
        // nobody asks.
        item.content_raw = content == *normalized ? std::nullopt : std::optional<std::string>{std::move(content)};
        item.content_sha256 = hash;
        item.grapheme_count = buffer->size();
        item.word_count = buffer->word_count();
        item.difficulty = core::difficulty_score(*normalized);
        item.created_at = clock_->unix_now();

        const core::Result<core::TextId> id = library_->add(item);
        if (!id) {
            return std::unexpected{id.error()};
        }
        return ImportOutcome{.id = *id, .already_present = false};
    }

    core::Status TextLibraryService::bookmark(core::TextId id, core::GraphemeIndex offset) {
        Bookmark mark;
        mark.text_id = id;
        mark.offset = offset;
        mark.updated_at = clock_->unix_now();
        return library_->set_bookmark(mark);
    }

}  // namespace typeit::app
