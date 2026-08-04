// The imported texts, in SQLite (TECHNICAL section 5).
//
// Implements the port `app` declares. The library is the other half of the
// database: history says how you typed, this says what you typed.
#ifndef TYPEIT_INFRA_DB_SQLITETEXTLIBRARYREPOSITORY_H
#define TYPEIT_INFRA_DB_SQLITETEXTLIBRARYREPOSITORY_H

#include <optional>
#include <string_view>
#include <vector>

#include "typeit/app/ports/ITextLibraryRepository.h"
#include "typeit/app/records/TextLibrary.h"
#include "typeit/core/util/Result.h"
#include "typeit/core/util/Units.h"
#include "typeit/infra/db/SqliteDatabase.h"

namespace typeit::infra {

    class SqliteTextLibraryRepository final : public app::ITextLibraryRepository {
    public:
        /// The database must outlive this and be migrated.
        explicit SqliteTextLibraryRepository(SqliteDatabase& database) : database_{&database} {}

        [[nodiscard]] core::Result<core::TextId> add(const app::TextItem& text) override;

        [[nodiscard]] core::Result<std::vector<app::TextSummary>> list(const app::TextFilter& filter) const override;

        [[nodiscard]] core::Result<std::optional<app::TextItem>> get(core::TextId id) const override;

        [[nodiscard]] core::Result<std::optional<app::TextItem>> find_by_hash(std::string_view sha256) const override;

        [[nodiscard]] core::Status remove(core::TextId id) override;

        [[nodiscard]] core::Status tag(core::TextId id, std::string_view tag) override;
        [[nodiscard]] core::Status untag(core::TextId id, std::string_view tag) override;

        [[nodiscard]] core::Status set_bookmark(const app::Bookmark& bookmark) override;
        [[nodiscard]] core::Result<std::optional<app::Bookmark>> bookmark(core::TextId id) const override;

    private:
        [[nodiscard]] core::Result<std::vector<std::string>> tags_of(core::TextId id) const;

        SqliteDatabase* database_;
    };

}  // namespace typeit::infra

#endif  // TYPEIT_INFRA_DB_SQLITETEXTLIBRARYREPOSITORY_H
