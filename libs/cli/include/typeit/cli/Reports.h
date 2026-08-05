// What `--stats` and `--export` print (TI-077).
//
// Both are the history, read once and written out; the difference is who is
// reading. `--export` is for a spreadsheet or a script and hands over every
// row. `--stats` is for a person at a terminal and hands over the four numbers
// they actually asked about.
//
// Neither computes anything: `HistoryService` already answers every question
// here, and a function that only forwards is a function that only forwards.
// What lives here is the part that is genuinely presentation — turning
// milliseconds into `1h 12m`, and deciding that an empty history deserves a
// sentence rather than a table of zeros.
#ifndef TYPEIT_CLI_REPORTS_H
#define TYPEIT_CLI_REPORTS_H

#include <string>

#include "typeit/app/ports/IHistoryRepository.h"
#include "typeit/app/records/History.h"
#include "typeit/app/services/HistoryService.h"
#include "typeit/cli/Cli.h"
#include "typeit/core/util/Result.h"
#include "typeit/core/util/Units.h"

namespace typeit::cli {

    /// The rows the command line asked for. `--last N` is a limit and nothing
    /// else: a filter that quietly excluded anything the user did not exclude
    /// would make two invocations disagree for no visible reason.
    [[nodiscard]] app::HistoryFilter filter_from(const CliOptions& options);

    /// The history as CSV or JSON, according to `options.operand`.
    ///
    /// The format was already checked by the parser, so an unrecognised one
    /// here is a bug rather than a user error — it is still reported rather
    /// than assumed away, because a silent default would export the wrong
    /// thing without saying so.
    [[nodiscard]] core::Result<std::string> export_history(const app::HistoryService& history,
                                                           const CliOptions& options);

    /// The summary a person reads: totals, streak, and the records set.
    ///
    /// `today` and `offset` are parameters for the same reason they are on
    /// `HistoryService::streak` — a streak is counted in local days, and
    /// neither the clock nor the timezone is this layer's to read.
    ///
    /// An empty history gets a sentence, not a table of zeros: somebody who
    /// has never typed does not need to be told their mean accuracy is 0%.
    [[nodiscard]] core::Result<std::string> stats_summary(const app::HistoryService& history,
                                                          const app::IHistoryRepository& repository,
                                                          const CliOptions& options, core::Millis today,
                                                          app::UtcOffsetMinutes offset = 0);

}  // namespace typeit::cli

#endif  // TYPEIT_CLI_REPORTS_H
