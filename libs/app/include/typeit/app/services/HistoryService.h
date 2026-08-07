// What the history screen and `--stats` read (TI-069, GAMEPLAY §7).
//
// Everything here is a question about runs that have already happened: how the
// numbers move over time, how many days in a row, and how to get it all back
// out of the program. Plain queries are not wrapped — the repository is
// already the port, and a method that only forwards is a method that only
// forwards.
#ifndef TYPEIT_APP_SERVICES_HISTORYSERVICE_H
#define TYPEIT_APP_SERVICES_HISTORYSERVICE_H

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "typeit/app/ports/IHistoryRepository.h"
#include "typeit/app/records/History.h"
#include "typeit/core/util/Result.h"
#include "typeit/core/util/Units.h"

namespace typeit::app {

    enum class TrendBucket : std::uint8_t {
        Day,
        Week,
    };

    /// One point of a trend line. Empty buckets are not emitted: a gap in the
    /// history is a gap, and drawing it as a run of zero-WPM days would say
    /// somebody typed badly on a day they did not type at all.
    struct TrendPoint {
        /// Local midnight that begins the bucket, in Unix milliseconds.
        core::Millis start{0};
        std::size_t sessions = 0;
        core::Wpm mean_net_wpm{0.0};
        core::Accuracy mean_accuracy{0.0};
        core::Millis total_time{0};
    };

    /// What counts as a day's typing done (GAMEPLAY §7.4).
    ///
    /// Either bar clears it: ten minutes and five runs are both a day somebody
    /// showed up, and requiring both would make the streak a chore rather than
    /// a record of showing up. `any()` is the no-goal case, where a single run
    /// is enough — which is what somebody who has turned the goal off wants,
    /// and what every caller wanted before there were goals.
    struct DailyGoal {
        core::Millis time{0};
        std::size_t runs = 0;

        [[nodiscard]] static DailyGoal any() { return DailyGoal{.time = core::Millis{0}, .runs = 1}; }

        /// Whether a day of `typed` across `runs` clears it.
        [[nodiscard]] bool met(core::Millis typed, std::size_t completed) const noexcept {
            if (time.value <= 0 && runs == 0) {
                // No goal set at all. Any run is a day.
                return completed > 0;
            }
            return (time.value > 0 && typed >= time) || (runs > 0 && completed >= runs);
        }
    };

    /// How one day is going against the goal.
    struct GoalProgress {
        core::Millis typed{0};
        std::size_t runs = 0;
        bool met = false;
    };

    struct Streak {
        /// Days in a row up to and including the most recent day typed. Zero
        /// when the last run was before yesterday — a streak that has already
        /// been broken is not a streak that is still running.
        std::size_t current = 0;
        std::size_t longest = 0;
    };

    class HistoryService {
    public:
        explicit HistoryService(IHistoryRepository& history) : history_{&history} {}

        /// Runs grouped into local days or weeks, oldest first.
        ///
        /// Weeks begin on Monday, which is what every other week in this
        /// project means.
        [[nodiscard]] core::Result<std::vector<TrendPoint>> trend(const HistoryFilter& filter, TrendBucket bucket,
                                                                  UtcOffsetMinutes offset = 0) const;

        /// Consecutive local days that **met the goal**, counted back from
        /// `today` (GAMEPLAY §7.4). Two runs on one day are one day.
        ///
        /// A run is attributed to the day it **started** in. One that begins at
        /// 23:58 and ends at 00:04 belongs to the day the typist sat down, not
        /// the one they got up in: the alternative attributes a run to a day
        /// its typist may never have been awake for, and `started_at` is the
        /// only one of the two the list is ordered by anyway.
        [[nodiscard]] core::Result<Streak> streak(const HistoryFilter& filter, core::Millis today,
                                                  UtcOffsetMinutes offset = 0, DailyGoal goal = DailyGoal::any()) const;

        /// Where today stands against the goal, for the screen that shows it.
        [[nodiscard]] core::Result<GoalProgress> today(const HistoryFilter& filter, core::Millis now,
                                                       UtcOffsetMinutes offset = 0,
                                                       DailyGoal goal = DailyGoal::any()) const;

        /// RFC 4180: comma separated, `"` doubled, and any field containing a
        /// comma, a quote or a newline quoted. `mode_param` is JSON and
        /// contains all three, so this is not a theoretical concern.
        [[nodiscard]] core::Result<std::string> to_csv(const HistoryFilter& filter) const;

        /// An array of objects. Non-ASCII passes through as UTF-8 rather than
        /// as `\u` escapes: the file is UTF-8 and every parser reads it.
        [[nodiscard]] core::Result<std::string> to_json(const HistoryFilter& filter) const;

    private:
        IHistoryRepository* history_;
    };

}  // namespace typeit::app

#endif  // TYPEIT_APP_SERVICES_HISTORYSERVICE_H
