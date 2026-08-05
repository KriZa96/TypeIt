// The history, in SQLite (TECHNICAL section 5).
//
// Implements the port `app` declares. Everything a run is remembered by goes
// through here, and every write is one transaction: a session and its samples
// and its personal bests are one fact about one run, and half of that fact is
// worse than none of it.
#ifndef TYPEIT_INFRA_DB_SQLITEHISTORYREPOSITORY_H
#define TYPEIT_INFRA_DB_SQLITEHISTORYREPOSITORY_H

#include <vector>

#include "typeit/app/ports/IHistoryRepository.h"
#include "typeit/app/records/History.h"
#include "typeit/core/metrics/ErrorMap.h"
#include "typeit/core/metrics/KeyStats.h"
#include "typeit/core/util/Result.h"
#include "typeit/core/util/Units.h"
#include "typeit/infra/db/SqliteDatabase.h"

namespace typeit::infra {

    class SqliteHistoryRepository final : public app::IHistoryRepository {
    public:
        /// The database must outlive this, be migrated, and be the only
        /// connection this process writes through (ARCHITECTURE section 6.4).
        explicit SqliteHistoryRepository(SqliteDatabase& database) : database_{&database} {}

        [[nodiscard]] core::Result<core::SessionId> save(const app::SessionRecord& record) override;

        [[nodiscard]] core::Result<core::SessionId> save_run(const app::SessionRecord& record,
                                                             const core::KeyStats& keys,
                                                             const core::ErrorMap& errors) override;

        [[nodiscard]] core::Result<std::vector<app::SessionRow>> query(const app::HistoryFilter& filter) const override;

        [[nodiscard]] core::Result<app::Aggregates> aggregates(const app::HistoryFilter& filter) const override;

        [[nodiscard]] core::Result<std::vector<app::PersonalBest>> personal_bests() const override;

        [[nodiscard]] core::Status merge_key_stats(const core::KeyStats& stats) override;

        [[nodiscard]] core::Result<core::KeyStats> key_stats(const app::HistoryFilter& filter) const override;

        [[nodiscard]] core::Status merge_error_map(const core::ErrorMap& errors) override;

        [[nodiscard]] core::Result<core::Wpm> best_sustained_wpm(core::Days window) const override;

        /// The accuracy a run needs before it can set a record
        /// (GAMEPLAY section 7.3). A personal best cannot be bought by typing
        /// nonsense quickly.
        static constexpr double kPersonalBestMinimumAccuracy = 0.90;

    private:
        /// Every write a finished run makes, inside a transaction the caller
        /// has already opened. SQLite has no nested transactions, so the split
        /// between "what to write" and "who owns the transaction" is the only
        /// way `save`, `save_run` and the two merges can share this code
        /// without any of them silently committing another's work.
        [[nodiscard]] core::Result<core::SessionId> write_run(const app::SessionRecord& record,
                                                              const core::KeyStats& keys, const core::ErrorMap& errors);

        [[nodiscard]] core::Result<core::SessionId> write_session(const app::SessionRecord& record);
        [[nodiscard]] core::Status write_samples(core::SessionId session, const app::SessionRecord& record);
        [[nodiscard]] core::Status update_personal_bests(core::SessionId session, const app::SessionRecord& record);
        [[nodiscard]] core::Status write_key_stats(const core::KeyStats& stats);
        [[nodiscard]] core::Status write_error_pairs(const core::ErrorMap& errors);

        SqliteDatabase* database_;
    };

}  // namespace typeit::infra

#endif  // TYPEIT_INFRA_DB_SQLITEHISTORYREPOSITORY_H
