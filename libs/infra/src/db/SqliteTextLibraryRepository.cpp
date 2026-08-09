#include "typeit/infra/db/SqliteTextLibraryRepository.h"

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "typeit/app/records/TextLibrary.h"
#include "typeit/core/util/Result.h"
#include "typeit/core/util/Units.h"
#include "typeit/infra/db/SqliteDatabase.h"

namespace typeit::infra {
    namespace {

        using core::ErrorCode;
        using core::Result;
        using core::Status;

        /// The tag filter as a JSON array, which SQLite's json_each turns back
        /// into rows. One bound parameter for any number of tags — the
        /// alternative is an IN list built by concatenation, and this codebase
        /// does not build SQL by concatenation.
        std::string tags_as_json(const std::vector<std::string>& tags) {
            std::string json = "[";
            for (const std::string& tag: tags) {
                if (json.size() > 1) {
                    json += ',';
                }
                json += '"';
                for (const char character: tag) {
                    // A tag is user text and may contain a quote or a backslash.
                    if (character == '"' || character == '\\') {
                        json += '\\';
                    }
                    json += character;
                }
                json += '"';
            }
            json += ']';
            return json;
        }

        Result<std::optional<app::TextItem>> read_one(Statement& statement) {
            const Result<bool> row = statement.step();
            if (!row) {
                return std::unexpected{row.error()};
            }
            if (!*row) {
                return std::nullopt;
            }

            app::TextItem text;
            text.id = core::TextId{statement.column_int(0)};
            text.title = statement.column_text(1);
            // A source the enum does not know cannot reach here: the column has a
            // CHECK constraint naming exactly the four, so anything else was
            // refused on the way in.
            text.source = app::text_source_from(statement.column_text(2)).value_or(app::TextSource::Paste);
            if (!statement.column_is_null(3)) {
                text.origin = statement.column_text(3);
            }
            text.content = statement.column_text(4);
            if (!statement.column_is_null(5)) {
                text.content_raw = statement.column_text(5);
            }
            text.content_sha256 = statement.column_text(6);
            if (!statement.column_is_null(7)) {
                text.language = statement.column_text(7);
            }
            text.grapheme_count = static_cast<std::size_t>(statement.column_int(8));
            text.word_count = static_cast<std::size_t>(statement.column_int(9));
            if (!statement.column_is_null(10)) {
                text.difficulty = statement.column_double(10);
            }
            text.created_at = core::Millis{statement.column_int(11)};
            if (!statement.column_is_null(12)) {
                text.author = statement.column_text(12);
            }
            if (!statement.column_is_null(13)) {
                text.mime = statement.column_text(13);
            }
            if (!statement.column_is_null(14)) {
                text.extractor = statement.column_text(14);
            }
            return text;
        }

    }  // namespace

    namespace {

        /// One nullable string, bound or nulled. Written once because the
        /// alternative is eight identical four-line branches, and the one that
        /// gets the parameter number wrong is the one nobody spots.
        void bind_optional(Statement& statement, int parameter, const std::optional<std::string>& value) {
            if (value.has_value()) {
                statement.bind(parameter, *value);
            } else {
                statement.bind_null(parameter);
            }
        }

    }  // namespace

    Result<core::TextId> SqliteTextLibraryRepository::add(const app::TextItem& text) {
        // The text and its sections go in together or not at all: a text with
        // its sections half written is one where "which chapter is this offset
        // in" has no answer, and the migration made sure that state does not
        // otherwise exist.
        Result<Transaction> transaction = database_->begin();
        if (!transaction) {
            return std::unexpected{transaction.error()};
        }

        Result<Statement> insert = database_->prepare(
                "INSERT INTO text_item (title, source, origin, content, content_raw, content_sha256, language,"
                " grapheme_count, word_count, difficulty, created_at, author, mime, extractor)"
                " VALUES (?1, ?2, ?3, ?4, ?5, ?6, ?7, ?8, ?9, ?10, ?11, ?12, ?13, ?14)");
        if (!insert) {
            return std::unexpected{insert.error()};
        }

        insert->bind(1, text.title)
                .bind(2, app::to_string(text.source))
                .bind(4, text.content)
                .bind(6, text.content_sha256)
                .bind(8, static_cast<std::int64_t>(text.grapheme_count))
                .bind(9, static_cast<std::int64_t>(text.word_count))
                .bind(11, text.created_at.value);

        bind_optional(*insert, 3, text.origin);
        bind_optional(*insert, 5, text.content_raw);
        bind_optional(*insert, 7, text.language);
        bind_optional(*insert, 12, text.author);
        bind_optional(*insert, 13, text.mime);
        bind_optional(*insert, 14, text.extractor);
        if (text.difficulty.has_value()) {
            insert->bind(10, *text.difficulty);
        } else {
            insert->bind_null(10);
        }

        if (const Status written = insert->run(); !written) {
            return std::unexpected{written.error()};
        }

        const Result<std::int64_t> id = database_->query_int("SELECT last_insert_rowid()");
        if (!id) {
            return std::unexpected{id.error()};
        }

        Result<Statement> section = database_->prepare(
                "INSERT INTO text_section (text_id, idx, title, start_idx, end_idx) VALUES (?1, ?2, ?3, ?4, ?5)");
        if (!section) {
            return std::unexpected{section.error()};
        }
        for (const app::TextSection& part: text.sections) {
            section->reset();
            section->bind(1, *id)
                    .bind(2, static_cast<std::int64_t>(part.idx))
                    .bind(4, static_cast<std::int64_t>(part.start.value))
                    .bind(5, static_cast<std::int64_t>(part.end.value));
            bind_optional(*section, 3, part.title);
            if (const Status written = section->run(); !written) {
                return std::unexpected{written.error()};
            }
        }

        if (const Status committed = transaction->commit(); !committed) {
            return std::unexpected{committed.error()};
        }
        return core::TextId{*id};
    }

    Result<std::optional<app::TextItem>> SqliteTextLibraryRepository::with_sections(
            Result<std::optional<app::TextItem>> text) const {
        if (!text || !text->has_value()) {
            return text;
        }
        Result<std::vector<app::TextSection>> sections = sections_of((*text)->id);
        if (!sections) {
            return std::unexpected{sections.error()};
        }
        (*text)->sections = std::move(*sections);
        return text;
    }

    Result<std::vector<app::TextSection>> SqliteTextLibraryRepository::sections_of(core::TextId id) const {
        Result<Statement> statement = database_->prepare(
                "SELECT idx, title, start_idx, end_idx FROM text_section WHERE text_id = ?1 ORDER BY idx");
        if (!statement) {
            return std::unexpected{statement.error()};
        }
        statement->bind(1, id.value);

        std::vector<app::TextSection> sections;
        for (;;) {
            const Result<bool> row = statement->step();
            if (!row) {
                return std::unexpected{row.error()};
            }
            if (!*row) {
                break;
            }
            app::TextSection section;
            section.idx = static_cast<std::size_t>(statement->column_int(0));
            if (!statement->column_is_null(1)) {
                section.title = statement->column_text(1);
            }
            section.start = core::GraphemeIndex{static_cast<std::size_t>(statement->column_int(2))};
            section.end = core::GraphemeIndex{static_cast<std::size_t>(statement->column_int(3))};
            sections.push_back(std::move(section));
        }
        return sections;
    }

    Result<std::optional<app::TextItem>> SqliteTextLibraryRepository::get(core::TextId id) const {
        // The column list is written out rather than shared between the two
        // queries that use it: assembling SQL from pieces is the habit the lint
        // exists to stop, and it does not distinguish a column list from a
        // value. Two literals is a small price for a rule with no exceptions.
        Result<Statement> statement = database_->prepare(
                "SELECT id, title, source, origin, content, content_raw, content_sha256, language, grapheme_count,"
                " word_count, difficulty, created_at, author, mime, extractor FROM text_item WHERE id = ?1");
        if (!statement) {
            return std::unexpected{statement.error()};
        }
        statement->bind(1, id.value);
        return with_sections(read_one(*statement));
    }

    Result<std::optional<app::TextItem>> SqliteTextLibraryRepository::find_by_hash(std::string_view sha256) const {
        Result<Statement> statement = database_->prepare(
                "SELECT id, title, source, origin, content, content_raw, content_sha256, language, grapheme_count,"
                " word_count, difficulty, created_at, author, mime, extractor FROM text_item"
                " WHERE content_sha256 = ?1");
        if (!statement) {
            return std::unexpected{statement.error()};
        }
        statement->bind(1, sha256);
        return with_sections(read_one(*statement));
    }

    Result<std::vector<std::string>> SqliteTextLibraryRepository::tags_of(core::TextId id) const {
        Result<Statement> statement = database_->prepare("SELECT tag FROM text_tag WHERE text_id = ?1 ORDER BY tag");
        if (!statement) {
            return std::unexpected{statement.error()};
        }
        statement->bind(1, id.value);

        std::vector<std::string> tags;
        for (;;) {
            const Result<bool> row = statement->step();
            if (!row) {
                return std::unexpected{row.error()};
            }
            if (!*row) {
                break;
            }
            tags.push_back(statement->column_text(0));
        }
        return tags;
    }

    Result<std::vector<app::TextSummary>> SqliteTextLibraryRepository::list(const app::TextFilter& filter) const {
        // The content is deliberately absent: a listing of a hundred texts
        // should not carry a hundred megabytes nobody is reading yet.
        //
        // The tag test counts distinct matches rather than testing membership,
        // so a text must carry *every* tag asked for.
        //
        // The *search* looks at the title and at the tags, because somebody who
        // tagged a text "rust" and called it something else will type "rust"
        // and expect to find it. It is an OR across the two and an AND with the
        // tag filter: the filter narrows, the search finds.
        //
        // LIKE is SQLite's, which means ASCII-only case folding — a search for
        // "Č" will not match "č", and fixing that needs ICU. An exact non-ASCII
        // match does work, which is most of what anybody types.
        Result<Statement> statement = database_->prepare(
                "SELECT t.id, t.title, t.source, t.origin, t.grapheme_count, t.word_count, t.difficulty,"
                " t.created_at FROM text_item t"
                " WHERE (?1 = '' OR t.title LIKE '%' || ?1 || '%'"
                "         OR EXISTS (SELECT 1 FROM text_tag s WHERE s.text_id = t.id"
                "                    AND s.tag LIKE '%' || ?1 || '%'))"
                "   AND (?2 = 0 OR (SELECT COUNT(DISTINCT g.tag) FROM text_tag g"
                "                   WHERE g.text_id = t.id AND g.tag IN (SELECT value FROM json_each(?3))) = ?2)"
                " ORDER BY t.created_at DESC, t.id DESC LIMIT ?4");
        if (!statement) {
            return std::unexpected{statement.error()};
        }

        statement->bind(1, filter.search.value_or(""))
                .bind(2, static_cast<std::int64_t>(filter.tags.size()))
                .bind(3, tags_as_json(filter.tags))
                .bind(4, filter.limit == 0 ? std::int64_t{-1} : static_cast<std::int64_t>(filter.limit));

        std::vector<app::TextSummary> summaries;
        for (;;) {
            const Result<bool> row = statement->step();
            if (!row) {
                return std::unexpected{row.error()};
            }
            if (!*row) {
                break;
            }

            app::TextSummary summary;
            summary.id = core::TextId{statement->column_int(0)};
            summary.title = statement->column_text(1);
            summary.source = app::text_source_from(statement->column_text(2)).value_or(app::TextSource::Paste);
            if (!statement->column_is_null(3)) {
                summary.origin = statement->column_text(3);
            }
            summary.grapheme_count = static_cast<std::size_t>(statement->column_int(4));
            summary.word_count = static_cast<std::size_t>(statement->column_int(5));
            if (!statement->column_is_null(6)) {
                summary.difficulty = statement->column_double(6);
            }
            summary.created_at = core::Millis{statement->column_int(7)};
            summaries.push_back(std::move(summary));
        }

        // Tags are read per text afterwards rather than joined and de-duplicated
        // in one query: a listing is a handful of rows, and one obvious query
        // per row beats one clever one nobody can read.
        for (app::TextSummary& summary: summaries) {
            Result<std::vector<std::string>> tags = tags_of(summary.id);
            if (!tags) {
                return std::unexpected{tags.error()};
            }
            summary.tags = std::move(*tags);
        }
        return summaries;
    }

    Status SqliteTextLibraryRepository::remove(core::TextId id) {
        // The tags and the bookmark go with it (ON DELETE CASCADE); the
        // sessions typed against it stay, with a null text (ON DELETE SET
        // NULL). Removing a text from the library is not disowning the runs.
        Result<Transaction> transaction = database_->begin();
        if (!transaction) {
            return std::unexpected{transaction.error()};
        }

        Result<Statement> statement = database_->prepare("DELETE FROM text_item WHERE id = ?1");
        if (!statement) {
            return std::unexpected{statement.error()};
        }
        statement->bind(1, id.value);
        if (const Status deleted = statement->run(); !deleted) {
            return deleted;
        }
        return transaction->commit();
    }

    Status SqliteTextLibraryRepository::tag(core::TextId id, std::string_view tag) {
        // Tagging twice is not an error; it is somebody clicking twice.
        Result<Statement> statement = database_->prepare(
                "INSERT INTO text_tag (text_id, tag) VALUES (?1, ?2)"
                " ON CONFLICT (text_id, tag) DO NOTHING");
        if (!statement) {
            return std::unexpected{statement.error()};
        }
        statement->bind(1, id.value).bind(2, tag);
        return statement->run();
    }

    Status SqliteTextLibraryRepository::untag(core::TextId id, std::string_view tag) {
        Result<Statement> statement = database_->prepare("DELETE FROM text_tag WHERE text_id = ?1 AND tag = ?2");
        if (!statement) {
            return std::unexpected{statement.error()};
        }
        statement->bind(1, id.value).bind(2, tag);
        return statement->run();
    }

    Status SqliteTextLibraryRepository::set_bookmark(const app::Bookmark& bookmark) {
        // One bookmark per text, updated in place: two bookmarks in one book is
        // a question with no good answer.
        Result<Statement> statement = database_->prepare(
                "INSERT INTO text_bookmark (text_id, offset, updated_at, section_idx) VALUES (?1, ?2, ?3, ?4)"
                " ON CONFLICT (text_id) DO UPDATE SET"
                "   offset = excluded.offset, updated_at = excluded.updated_at,"
                "   section_idx = excluded.section_idx");
        if (!statement) {
            return std::unexpected{statement.error()};
        }
        statement->bind(1, bookmark.text_id.value)
                .bind(2, static_cast<std::int64_t>(bookmark.offset.value))
                .bind(3, bookmark.updated_at.value)
                .bind(4, static_cast<std::int64_t>(bookmark.section_idx));
        return statement->run();
    }

    Result<std::optional<app::Bookmark>> SqliteTextLibraryRepository::bookmark(core::TextId id) const {
        Result<Statement> statement = database_->prepare(
                "SELECT text_id, offset, updated_at, section_idx FROM text_bookmark WHERE text_id = ?1");
        if (!statement) {
            return std::unexpected{statement.error()};
        }
        statement->bind(1, id.value);

        const Result<bool> row = statement->step();
        if (!row) {
            return std::unexpected{row.error()};
        }
        if (!*row) {
            return std::nullopt;
        }
        return app::Bookmark{
                .text_id = core::TextId{statement->column_int(0)},
                .offset = core::GraphemeIndex{static_cast<std::size_t>(statement->column_int(1))},
                .updated_at = core::Millis{statement->column_int(2)},
                .section_idx = static_cast<std::size_t>(statement->column_int(3)),
        };
    }

}  // namespace typeit::infra
