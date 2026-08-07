// What a finished run looks like on its way to storage, and on its way back
// (TECHNICAL section 5).
//
// These are the application's shapes, not the domain's: `core` computes
// metrics and knows nothing about rows, and `infra` writes rows and knows
// nothing about typing. The records in the middle belong to neither, which is
// why they live here.
#ifndef TYPEIT_APP_RECORDS_HISTORY_H
#define TYPEIT_APP_RECORDS_HISTORY_H

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "typeit/core/metrics/Timeline.h"
#include "typeit/core/util/Units.h"

namespace typeit::app {

    /// A run, complete, as it is about to be written. Everything here is
    /// already computed: saving is a write, not a calculation.
    struct SessionRecord {
        /// Unix milliseconds, unlike the log's own clock, which starts at an
        /// arbitrary point. A history spanning machines needs a real date.
        core::Millis started_at{0};
        core::Millis ended_at{0};

        std::string mode;
        /// JSON — `{"seconds":30}`, `{"words":50}`, the race parameters. A
        /// column per mode would mean a migration per mode (ADR-008).
        std::string mode_param;

        /// Absent for generated text with nothing in the library behind it.
        std::optional<core::TextId> text_id;
        std::string provider;
        /// Reproduces the exact text stream, which turns a bug report into a
        /// deterministic repro.
        std::uint64_t provider_seed = 0;

        core::Millis duration{0};
        std::size_t graphemes_typed = 0;
        std::size_t graphemes_correct = 0;
        std::size_t errors_total = 0;
        std::size_t errors_uncorrected = 0;
        std::size_t backspaces = 0;

        core::Wpm raw_wpm{0.0};
        core::Wpm gross_wpm{0.0};
        core::Wpm net_wpm{0.0};
        core::Accuracy accuracy{0.0};
        core::Accuracy final_correctness{0.0};
        /// The 0–100 scale the metric is quoted on (GAMEPLAY section 4.3).
        double consistency = 0.0;

        /// Race only: the highest sustained pacer speed, and the speed at which
        /// accuracy collapsed.
        std::optional<core::Wpm> peak_wpm;
        std::optional<core::Wpm> wall_wpm;

        /// Abandoned runs are saved and never set a record
        /// (GAMEPLAY section 7.1).
        bool completed = false;
        std::string app_version;

        /// Per-second samples, written alongside the session in the same
        /// transaction.
        std::vector<core::TimelineSample> timeline;
    };

    /// A run as the history screen reads it: the headline numbers and the id to
    /// drill into, without the timeline nobody has asked for yet.
    struct SessionRow {
        core::SessionId id{0};
        core::Millis started_at{0};
        std::string mode;
        std::string mode_param;
        core::Millis duration{0};
        core::Wpm net_wpm{0.0};
        core::Wpm gross_wpm{0.0};
        core::Accuracy accuracy{0.0};
        double consistency = 0.0;
        bool completed = false;
    };

    /// Which runs a query is about. Every field absent means "all of them",
    /// which is what the history screen opens with.
    struct HistoryFilter {
        std::optional<std::string> mode;
        /// Inclusive lower bound, exclusive upper — the same half-open
        /// convention as the timeline buckets, so a run on a boundary belongs
        /// to exactly one range.
        std::optional<core::Millis> since;
        std::optional<core::Millis> until;
        /// Zero means unlimited. A history screen asks for a page; an export
        /// asks for everything.
        std::size_t limit = 0;
        /// Abandoned runs are excluded from trends by default and included when
        /// somebody wants to see everything they did.
        bool completed_only = true;
    };

    /// Minutes east of UTC. Days are local days: a run at 23:30 and a run at
    /// 00:30 are two days to the person who did them, whatever UTC thinks.
    ///
    /// A parameter rather than something read from the machine because neither
    /// `app` nor `infra` has any business calling the operating system — the
    /// composition root passes the real offset, and a test passes whichever
    /// one it is asking about.
    using UtcOffsetMinutes = std::int32_t;

    /// One local day's runs, already summed. The unit the trend, the streak and
    /// the daily goal are all counted in.
    ///
    /// Aggregated by the database rather than by loading rows and adding them
    /// up here: a year of history is at most 366 of these, where the rows
    /// behind them are however many runs somebody has done (TI-109).
    struct DayBucket {
        /// Days since 1 January 1970, in the caller's local time.
        std::int64_t day = 0;
        std::size_t sessions = 0;
        core::Wpm mean_net_wpm{0.0};
        core::Accuracy mean_accuracy{0.0};
        core::Millis total_time{0};
    };

    /// Zeros rather than NaN over an empty range: a new user's history screen
    /// is a normal thing to draw.
    struct Aggregates {
        std::size_t sessions = 0;
        core::Wpm mean_net_wpm{0.0};
        core::Wpm best_net_wpm{0.0};
        core::Wpm worst_net_wpm{0.0};
        core::Accuracy mean_accuracy{0.0};
        core::Millis total_time{0};
        std::size_t total_graphemes = 0;
    };

    /// One record, per `(mode, parameter, metric)` — a 15-second best and a
    /// 60-second best are different records because they measure different
    /// things (GAMEPLAY section 7.3).
    struct PersonalBest {
        std::string mode;
        std::string param;
        std::string metric;
        core::SessionId session_id{0};
        double value = 0.0;
        core::Millis achieved_at{0};
    };

}  // namespace typeit::app

#endif  // TYPEIT_APP_RECORDS_HISTORY_H
