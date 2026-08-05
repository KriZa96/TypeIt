#include "typeit/cli/Reports.h"

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

#include "typeit/app/Json.h"
#include "typeit/app/ports/IHistoryRepository.h"
#include "typeit/app/records/History.h"
#include "typeit/app/services/HistoryService.h"
#include "typeit/cli/Cli.h"
#include "typeit/core/util/Result.h"
#include "typeit/core/util/Units.h"

namespace typeit::cli {
    namespace {

        constexpr std::int64_t kMillisPerSecond = 1'000;
        constexpr std::int64_t kSecondsPerMinute = 60;
        constexpr std::int64_t kMinutesPerHour = 60;

        /// `1h 12m`, `12m 30s`, `9s`. Hours are dropped when there are none and
        /// seconds when there are hours, because "1h 12m 03s" is a stopwatch
        /// and this is a total.
        std::string duration_text(core::Millis total) {
            const std::int64_t seconds = total.value / kMillisPerSecond;
            const std::int64_t minutes = seconds / kSecondsPerMinute;
            const std::int64_t hours = minutes / kMinutesPerHour;

            if (hours > 0) {
                return std::to_string(hours) + "h " + std::to_string(minutes % kMinutesPerHour) + "m";
            }
            if (minutes > 0) {
                return std::to_string(minutes) + "m " + std::to_string(seconds % kSecondsPerMinute) + "s";
            }
            return std::to_string(seconds) + "s";
        }

        /// A ratio as a percentage. `app::json::number` rather than a second
        /// spelling of the same thing: `--stats` and `--export` reporting one
        /// run's accuracy as 96.3 and 96.300000 would be a difference nobody
        /// could explain.
        std::string percent_text(core::Accuracy accuracy) { return app::json::number(accuracy.value * 100.0) + "%"; }

        void append_line(std::string& out, std::string_view key, std::string_view value) {
            out += key;
            out += ": ";
            out += value;
            out += '\n';
        }

        std::string days_text(std::size_t days) { return std::to_string(days) + (days == 1 ? " day" : " days"); }

    }  // namespace

    app::HistoryFilter filter_from(const CliOptions& options) {
        app::HistoryFilter filter;
        if (options.last.has_value()) {
            filter.limit = static_cast<std::size_t>(*options.last);
        }
        return filter;
    }

    core::Result<std::string> export_history(const app::HistoryService& history, const CliOptions& options) {
        const app::HistoryFilter filter = filter_from(options);
        if (options.operand == "csv") {
            return history.to_csv(filter);
        }
        if (options.operand == "json") {
            return history.to_json(filter);
        }
        // The parser already refused anything else, so this is a bug rather
        // than a user error — and still reported, because a silent default
        // would export the wrong thing without saying so.
        return core::fail(core::ErrorCode::InvalidArgument, "--export = \"" + options.operand + "\"");
    }

    core::Result<std::string> stats_summary(const app::HistoryService& history,
                                            const app::IHistoryRepository& repository, const CliOptions& options,
                                            core::Millis today, app::UtcOffsetMinutes offset) {
        const app::HistoryFilter filter = filter_from(options);

        const core::Result<app::Aggregates> totals = repository.aggregates(filter);
        if (!totals) {
            return std::unexpected{totals.error()};
        }

        if (totals->sessions == 0) {
            // A sentence, not a table of zeros: somebody who has never typed
            // does not need to be told their mean accuracy is 0%.
            return std::string{"No sessions yet. Finish a run and it will show up here.\n"};
        }

        const core::Result<app::Streak> streak = history.streak(filter, today, offset);
        if (!streak) {
            return std::unexpected{streak.error()};
        }
        const core::Result<std::vector<app::PersonalBest>> bests = repository.personal_bests();
        if (!bests) {
            return std::unexpected{bests.error()};
        }

        std::string out = "[totals]\n";
        append_line(out, "sessions", std::to_string(totals->sessions));
        append_line(out, "time", duration_text(totals->total_time));
        append_line(out, "graphemes", std::to_string(totals->total_graphemes));
        append_line(out, "mean wpm", app::json::number(totals->mean_net_wpm.value));
        append_line(out, "best wpm", app::json::number(totals->best_net_wpm.value));
        append_line(out, "worst wpm", app::json::number(totals->worst_net_wpm.value));
        append_line(out, "mean accuracy", percent_text(totals->mean_accuracy));

        out += "\n[streak]\n";
        append_line(out, "current", days_text(streak->current));
        append_line(out, "longest", days_text(streak->longest));

        out += "\n[personal bests]\n";
        if (bests->empty()) {
            out += "none yet\n";
        }
        for (const app::PersonalBest& best: *bests) {
            // Mode, parameter and metric together, because a 15-second best and
            // a 60-second best are different records (GAMEPLAY section 7.3) and
            // a line that said only "net_wpm" would collapse them.
            //
            // No date: formatting one needs a calendar this layer has no
            // business owning, and `--export` carries the raw numbers for
            // anyone who wants more.
            append_line(out, best.mode + " " + best.param + " " + best.metric, app::json::number(best.value));
        }
        return out;
    }

}  // namespace typeit::cli
