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

    struct Streak {
        /// Days in a row up to and including the most recent day typed. Zero
        /// when the last run was before yesterday — a streak that has already
        /// been broken is not a streak that is still running.
        std::size_t current = 0;
        std::size_t longest = 0;
    };

    /// Minutes east of UTC. Days are local days: a run at 23:30 and a run at
    /// 00:30 are two days to the person who did them, whatever UTC thinks.
    ///
    /// It is a parameter rather than something read from the machine because
    /// `app` has no business calling the operating system — the composition
    /// root passes the real offset, and a test passes whichever one it is
    /// asking about.
    using UtcOffsetMinutes = std::int32_t;

    class HistoryService {
    public:
        explicit HistoryService(IHistoryRepository& history) : history_{&history} {}

        /// Runs grouped into local days or weeks, oldest first.
        ///
        /// Weeks begin on Monday, which is what every other week in this
        /// project means.
        [[nodiscard]] core::Result<std::vector<TrendPoint>> trend(const HistoryFilter& filter, TrendBucket bucket,
                                                                  UtcOffsetMinutes offset = 0) const;

        /// Consecutive local days with at least one run, counted back from
        /// `today`. Two runs on one day are one day.
        [[nodiscard]] core::Result<Streak> streak(const HistoryFilter& filter, core::Millis today,
                                                  UtcOffsetMinutes offset = 0) const;

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
