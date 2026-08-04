// The imported texts (TECHNICAL section 2.1).
#ifndef TYPEIT_APP_PORTS_ITEXTLIBRARYREPOSITORY_H
#define TYPEIT_APP_PORTS_ITEXTLIBRARYREPOSITORY_H

#include <optional>
#include <string_view>
#include <vector>

#include "typeit/app/records/TextLibrary.h"
#include "typeit/core/util/Result.h"
#include "typeit/core/util/Units.h"

namespace typeit::app {

    class ITextLibraryRepository {
    public:
        ITextLibraryRepository() = default;
        virtual ~ITextLibraryRepository();
        ITextLibraryRepository(const ITextLibraryRepository&) = delete;
        ITextLibraryRepository& operator=(const ITextLibraryRepository&) = delete;
        ITextLibraryRepository(ITextLibraryRepository&&) = delete;
        ITextLibraryRepository& operator=(ITextLibraryRepository&&) = delete;

        /// The id the text was stored under. Importing the same content twice
        /// is one text: the caller checks `find_by_hash` first, and the unique
        /// constraint is there for the caller who forgets.
        [[nodiscard]] virtual core::Result<core::TextId> add(const TextItem& text) = 0;

        [[nodiscard]] virtual core::Result<std::vector<TextSummary>> list(const TextFilter& filter) const = 0;

        /// With its content, unlike `list`. Absent rather than an error when
        /// there is no such text: asking about a text that has been removed is
        /// a normal race, not a failure.
        [[nodiscard]] virtual core::Result<std::optional<TextItem>> get(core::TextId id) const = 0;

        [[nodiscard]] virtual core::Result<std::optional<TextItem>> find_by_hash(std::string_view sha256) const = 0;

        /// Takes the tags and the bookmark with it, and leaves the sessions
        /// typed against it alone with a null text.
        [[nodiscard]] virtual core::Status remove(core::TextId id) = 0;

        [[nodiscard]] virtual core::Status tag(core::TextId id, std::string_view tag) = 0;
        [[nodiscard]] virtual core::Status untag(core::TextId id, std::string_view tag) = 0;

        /// Replaces the text's bookmark. One per text: two bookmarks in one
        /// book is a question with no good answer.
        [[nodiscard]] virtual core::Status set_bookmark(const Bookmark& bookmark) = 0;

        [[nodiscard]] virtual core::Result<std::optional<Bookmark>> bookmark(core::TextId id) const = 0;
    };

}  // namespace typeit::app

#endif  // TYPEIT_APP_PORTS_ITEXTLIBRARYREPOSITORY_H
