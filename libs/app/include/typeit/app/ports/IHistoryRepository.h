// Everything a finished run has to be remembered by (TECHNICAL section 2.1).
//
// A port: `app` declares it, `infra` implements it, and neither knows the
// other exists (ADR-001). The fake in tests/support implements the same
// interface, and TI-063's contract suite runs both through the same
// expectations so they cannot quietly diverge.
#ifndef TYPEIT_APP_PORTS_IHISTORYREPOSITORY_H
#define TYPEIT_APP_PORTS_IHISTORYREPOSITORY_H

#include <vector>

#include "typeit/app/records/History.h"
#include "typeit/core/metrics/ErrorMap.h"
#include "typeit/core/metrics/KeyStats.h"
#include "typeit/core/util/Result.h"
#include "typeit/core/util/Units.h"

namespace typeit::app {

    class IHistoryRepository {
    public:
        IHistoryRepository() = default;
        virtual ~IHistoryRepository();
        IHistoryRepository(const IHistoryRepository&) = delete;
        IHistoryRepository& operator=(const IHistoryRepository&) = delete;
        IHistoryRepository(IHistoryRepository&&) = delete;
        IHistoryRepository& operator=(IHistoryRepository&&) = delete;

        /// The session and its samples, in one transaction. A failure leaves
        /// the history exactly as it was — a run is saved whole or not at all.
        [[nodiscard]] virtual core::Result<core::SessionId> save(const SessionRecord& record) = 0;

        /// Everything a finished run leaves behind — the session, its samples,
        /// the records it set, and the key, bigram and error-pair totals it
        /// adds to — in **one** transaction.
        ///
        /// This exists because `save` followed by `merge_key_stats` followed by
        /// `merge_error_map` cannot be made atomic from above: each is its own
        /// transaction, so a failure or a crash between two of them leaves a
        /// session row whose lifetime totals are missing, or totals for a run
        /// that is not in the history. Both are silently wrong forever after,
        /// and neither is recoverable by retrying — the retry would count the
        /// stats twice.
        ///
        /// `app::SessionService` saves through this and through nothing else.
        /// The narrower methods remain for the cases that are genuinely one
        /// write: seeding, and a repair tool merging stats recomputed from a
        /// keystroke log.
        [[nodiscard]] virtual core::Result<core::SessionId> save_run(const SessionRecord& record,
                                                                     const core::KeyStats& keys,
                                                                     const core::ErrorMap& errors) = 0;

        [[nodiscard]] virtual core::Result<std::vector<SessionRow>> query(const HistoryFilter& filter) const = 0;

        /// One run, with everything that was stored about it — including the
        /// per-second samples, which `SessionRow` deliberately omits.
        ///
        /// Separate from `query` rather than a flag on it because the two have
        /// opposite shapes: a list wants many rows and no samples, and a detail
        /// view wants one row and all of them. A `query` that returned samples
        /// would load three hundred thousand of them to draw a list of forty.
        ///
        /// `ErrorCode::NotFound` for an id nobody recorded, rather than an
        /// empty optional the caller might read as "no error".
        [[nodiscard]] virtual core::Result<SessionRecord> session(core::SessionId id) const = 0;

        /// Zeros over an empty range, not NaN: a new user's history screen is a
        /// normal thing to draw.
        [[nodiscard]] virtual core::Result<Aggregates> aggregates(const HistoryFilter& filter) const = 0;

        /// The matching runs summed per local day, oldest first.
        ///
        /// A port method rather than client-side grouping because it is an
        /// aggregate over the whole history and that belongs in SQL (TI-109).
        /// Loading every row to add them up here makes the memory a history
        /// screen needs grow with how much somebody has typed, which is exactly
        /// backwards: the answer is at most 366 buckets a year however many
        /// runs are behind it.
        ///
        /// Days that nobody typed on are **not** emitted. A gap is a gap, and
        /// filling it with zero-WPM days would say somebody typed badly on a
        /// day they did not type at all.
        [[nodiscard]] virtual core::Result<std::vector<DayBucket>> daily_totals(const HistoryFilter& filter,
                                                                                UtcOffsetMinutes offset) const = 0;

        [[nodiscard]] virtual core::Result<std::vector<PersonalBest>> personal_bests() const = 0;

        /// Adds this run's counts to the lifetime totals. Merging, not
        /// replacing: the per-key history is the sum of every run.
        [[nodiscard]] virtual core::Status merge_key_stats(const core::KeyStats& stats) = 0;

        [[nodiscard]] virtual core::Result<core::KeyStats> key_stats(const HistoryFilter& filter) const = 0;

        [[nodiscard]] virtual core::Status merge_error_map(const core::ErrorMap& errors) = 0;

        /// Race mode's starting speed comes from here (GAMEPLAY section 3.4).
        /// A port method rather than client-side filtering because it is a
        /// maximum over a large history, and that belongs in SQL.
        ///
        /// `min_accuracy` is the bar a run has to have cleared to count. A
        /// speed reached while typing badly is not a speed anybody held, and
        /// starting tomorrow's race from it would mean re-losing it every time
        /// (TI-126). It travels as a parameter for the same reason the window
        /// does: it is a domain rule, and infra's job is the maximum.
        [[nodiscard]] virtual core::Result<core::Wpm> best_sustained_wpm(core::Days window,
                                                                         core::Accuracy min_accuracy) const = 0;
    };

}  // namespace typeit::app

#endif  // TYPEIT_APP_PORTS_IHISTORYREPOSITORY_H
