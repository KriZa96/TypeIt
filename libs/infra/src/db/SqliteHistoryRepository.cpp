#include "typeit/infra/db/SqliteHistoryRepository.h"

#include <array>
#include <cstdint>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "typeit/app/records/History.h"
#include "typeit/core/metrics/ErrorMap.h"
#include "typeit/core/metrics/KeyStats.h"
#include "typeit/core/util/Result.h"
#include "typeit/core/util/Units.h"
#include "typeit/infra/db/SqliteDatabase.h"

namespace typeit::infra {
    namespace {

        using core::ErrorCode;
        using core::Result;
        using core::Status;

        constexpr std::int64_t kMillisPerDay = 86'400'000;

        /// Every filter is expressed as four bound parameters with neutral
        /// values, so one prepared statement serves every combination. The
        /// alternative is building the WHERE clause by concatenation, which is
        /// the thing this codebase does not do.
        struct BoundFilter {
            std::string mode;  ///< empty means every mode
            std::int64_t since = 0;
            std::int64_t until = 0;  ///< zero means no upper bound
            std::int64_t completed_only = 0;
        };

        BoundFilter bind_values(const app::HistoryFilter& filter) {
            return BoundFilter{
                    .mode = filter.mode.value_or(""),
                    .since = filter.since.value_or(core::Millis{0}).value,
                    .until = filter.until.value_or(core::Millis{0}).value,
                    .completed_only = filter.completed_only ? 1 : 0,
            };
        }

        void bind_filter(Statement& statement, const BoundFilter& values) {
            statement.bind(1, values.mode).bind(2, values.since).bind(3, values.until).bind(4, values.completed_only);
        }

        /// `0` means unlimited, which SQLite spells `-1`.
        std::int64_t limit_of(const app::HistoryFilter& filter) {
            return filter.limit == 0 ? -1 : static_cast<std::int64_t>(filter.limit);
        }

    }  // namespace

    Result<core::SessionId> SqliteHistoryRepository::save(const app::SessionRecord& record) {
        // One transaction for the session, its samples and its records. A run
        // is one fact; half of it is worse than none of it.
        return save_run(record, {}, {});
    }

    Result<core::SessionId> SqliteHistoryRepository::save_run(const app::SessionRecord& record,
                                                              const core::KeyStats& keys,
                                                              const core::ErrorMap& errors) {
        Result<Transaction> transaction = database_->begin();
        if (!transaction) {
            return std::unexpected{transaction.error()};
        }

        const Result<core::SessionId> session = write_run(record, keys, errors);
        if (!session) {
            // The transaction's destructor rolls back. Returning here rather
            // than committing is the whole guarantee: a failure at the fourth
            // write undoes the first three.
            return std::unexpected{session.error()};
        }

        if (const Status committed = transaction->commit(); !committed) {
            return std::unexpected{committed.error()};
        }
        return *session;
    }

    Result<core::SessionId> SqliteHistoryRepository::write_run(const app::SessionRecord& record,
                                                               const core::KeyStats& keys,
                                                               const core::ErrorMap& errors) {
        const Result<core::SessionId> session = write_session(record);
        if (!session) {
            return session;
        }
        if (const Status samples = write_samples(*session, record); !samples) {
            return std::unexpected{samples.error()};
        }
        if (const Status bests = update_personal_bests(*session, record); !bests) {
            return std::unexpected{bests.error()};
        }
        if (const Status merged = write_key_stats(keys); !merged) {
            return std::unexpected{merged.error()};
        }
        if (const Status merged = write_error_pairs(errors); !merged) {
            return std::unexpected{merged.error()};
        }
        return session;
    }

    Result<core::SessionId> SqliteHistoryRepository::write_session(const app::SessionRecord& record) {
        Result<Statement> insert = database_->prepare(
                "INSERT INTO session (started_at, ended_at, mode, mode_param, text_id, provider, provider_seed,"
                " duration_ms, graphemes_typed, graphemes_correct, errors_total, errors_uncorrected, backspaces,"
                " raw_wpm, gross_wpm, net_wpm, accuracy, final_correctness, consistency, peak_wpm, wall_wpm,"
                " completed, app_version)"
                " VALUES (?1, ?2, ?3, ?4, ?5, ?6, ?7, ?8, ?9, ?10, ?11, ?12, ?13, ?14, ?15, ?16, ?17, ?18, ?19,"
                " ?20, ?21, ?22, ?23)");
        if (!insert) {
            return std::unexpected{insert.error()};
        }

        insert->bind(1, record.started_at.value)
                .bind(2, record.ended_at.value)
                .bind(3, record.mode)
                .bind(4, record.mode_param)
                .bind(6, record.provider)
                .bind(7, static_cast<std::int64_t>(record.provider_seed))
                .bind(8, record.duration.value)
                .bind(9, static_cast<std::int64_t>(record.graphemes_typed))
                .bind(10, static_cast<std::int64_t>(record.graphemes_correct))
                .bind(11, static_cast<std::int64_t>(record.errors_total))
                .bind(12, static_cast<std::int64_t>(record.errors_uncorrected))
                .bind(13, static_cast<std::int64_t>(record.backspaces))
                .bind(14, record.raw_wpm.value)
                .bind(15, record.gross_wpm.value)
                .bind(16, record.net_wpm.value)
                .bind(17, record.accuracy.value)
                .bind(18, record.final_correctness.value)
                .bind(19, record.consistency)
                .bind(22, record.completed ? std::int64_t{1} : std::int64_t{0})
                .bind(23, record.app_version);

        if (record.text_id.has_value()) {
            insert->bind(5, record.text_id->value);
        } else {
            insert->bind_null(5);
        }
        if (record.peak_wpm.has_value()) {
            insert->bind(20, record.peak_wpm->value);
        } else {
            insert->bind_null(20);
        }
        if (record.wall_wpm.has_value()) {
            insert->bind(21, record.wall_wpm->value);
        } else {
            insert->bind_null(21);
        }

        if (const Status written = insert->run(); !written) {
            return std::unexpected{written.error()};
        }

        const Result<std::int64_t> id = database_->query_int("SELECT last_insert_rowid()");
        if (!id) {
            return std::unexpected{id.error()};
        }
        return core::SessionId{*id};
    }

    Status SqliteHistoryRepository::write_samples(core::SessionId session, const app::SessionRecord& record) {
        if (record.timeline.empty()) {
            return {};
        }
        Result<Statement> insert = database_->prepare(
                "INSERT INTO session_sample (session_id, t_ms, wpm, errors, pacer_wpm)"
                " VALUES (?1, ?2, ?3, ?4, ?5)");
        if (!insert) {
            return std::unexpected{insert.error()};
        }

        for (const core::TimelineSample& sample: record.timeline) {
            insert->reset();
            insert->bind(1, session.value)
                    .bind(2, sample.at.value)
                    .bind(3, sample.wpm.value)
                    .bind(4, static_cast<std::int64_t>(sample.errors));
            if (sample.pacer_wpm.has_value()) {
                insert->bind(5, sample.pacer_wpm->value);
            } else {
                insert->bind_null(5);
            }
            if (const Status written = insert->run(); !written) {
                return written;
            }
        }
        return {};
    }

    Status SqliteHistoryRepository::update_personal_bests(core::SessionId session, const app::SessionRecord& record) {
        // GAMEPLAY section 7.3: an abandoned run never sets a record, and
        // neither does one below the accuracy floor — a personal best cannot be
        // bought by typing nonsense quickly.
        if (!record.completed || record.accuracy.value < kPersonalBestMinimumAccuracy) {
            return {};
        }

        Result<Statement> upsert = database_->prepare(
                "INSERT INTO personal_best (mode, param, metric, session_id, value, achieved_at)"
                " VALUES (?1, ?2, ?3, ?4, ?5, ?6)"
                " ON CONFLICT (mode, param, metric) DO UPDATE SET"
                "   session_id = excluded.session_id,"
                "   value = excluded.value,"
                "   achieved_at = excluded.achieved_at"
                " WHERE excluded.value > personal_best.value");
        if (!upsert) {
            return std::unexpected{upsert.error()};
        }

        // Strictly greater, so an equal run leaves the older record standing:
        // the first person to get there keeps it, which is the documented
        // tie-break and the one that does not churn the achieved_at date.
        struct Candidate {
            std::string_view metric;
            double value;
            bool present;
        };
        // The extra braces are std::array's aggregate wrapping its C array;
        // designated initialisers cannot cross that boundary.
        const std::array<Candidate, 3> candidates{{
                Candidate{.metric = "net_wpm", .value = record.net_wpm.value, .present = true},
                Candidate{.metric = "accuracy", .value = record.accuracy.value, .present = true},
                Candidate{.metric = "peak_wpm",
                          .value = record.peak_wpm.value_or(core::Wpm{0.0}).value,
                          .present = record.peak_wpm.has_value()},
        }};

        for (const Candidate& candidate: candidates) {
            if (!candidate.present) {
                continue;
            }
            upsert->reset();
            upsert->bind(1, record.mode)
                    .bind(2, record.mode_param)
                    .bind(3, candidate.metric)
                    .bind(4, session.value)
                    .bind(5, candidate.value)
                    .bind(6, record.ended_at.value);
            if (const Status written = upsert->run(); !written) {
                return written;
            }
        }
        return {};
    }

    Result<std::vector<app::SessionRow>> SqliteHistoryRepository::query(const app::HistoryFilter& filter) const {
        // The WHERE clause is written out in both queries rather than shared
        // as a fragment. Assembling SQL from pieces is the habit the lint
        // exists to stop, and a lint that reads one line at a time would not
        // have caught this one — better to keep the rule absolute than to rely
        // on how far the check happens to see.
        Result<Statement> statement = database_->prepare(
                "SELECT id, started_at, mode, mode_param, duration_ms, net_wpm, gross_wpm, accuracy, consistency,"
                " completed FROM session"
                " WHERE (?1 = '' OR mode = ?1)"
                "   AND (?2 = 0 OR started_at >= ?2)"
                "   AND (?3 = 0 OR started_at < ?3)"
                "   AND (?4 = 0 OR completed = 1)"
                " ORDER BY started_at DESC, id DESC LIMIT ?5");
        if (!statement) {
            return std::unexpected{statement.error()};
        }

        bind_filter(*statement, bind_values(filter));
        statement->bind(5, limit_of(filter));

        std::vector<app::SessionRow> rows;
        for (;;) {
            const Result<bool> row = statement->step();
            if (!row) {
                return std::unexpected{row.error()};
            }
            if (!*row) {
                break;
            }
            rows.push_back(app::SessionRow{
                    .id = core::SessionId{statement->column_int(0)},
                    .started_at = core::Millis{statement->column_int(1)},
                    .mode = statement->column_text(2),
                    .mode_param = statement->column_text(3),
                    .duration = core::Millis{statement->column_int(4)},
                    .net_wpm = core::Wpm{statement->column_double(5)},
                    .gross_wpm = core::Wpm{statement->column_double(6)},
                    .accuracy = core::Accuracy{statement->column_double(7)},
                    .consistency = statement->column_double(8),
                    .completed = statement->column_int(9) != 0,
            });
        }
        return rows;
    }

    Result<app::SessionRecord> SqliteHistoryRepository::session(core::SessionId id) const {
        Result<Statement> statement = database_->prepare(
                "SELECT started_at, ended_at, mode, mode_param, text_id, provider, provider_seed, duration_ms,"
                " graphemes_typed, graphemes_correct, errors_total, errors_uncorrected, backspaces, raw_wpm,"
                " gross_wpm, net_wpm, accuracy, final_correctness, consistency, peak_wpm, wall_wpm, completed,"
                " app_version FROM session WHERE id = ?1");
        if (!statement) {
            return std::unexpected{statement.error()};
        }
        statement->bind(1, id.value);

        const Result<bool> found = statement->step();
        if (!found) {
            return std::unexpected{found.error()};
        }
        if (!*found) {
            // The database answered, and the answer was "no such run" — which
            // is a different thing from a query it could not run.
            return core::fail(core::ErrorCode::SessionNotFound, std::to_string(id.value));
        }

        app::SessionRecord record;
        record.started_at = core::Millis{statement->column_int(0)};
        record.ended_at = core::Millis{statement->column_int(1)};
        record.mode = statement->column_text(2);
        record.mode_param = statement->column_text(3);
        if (!statement->column_is_null(4)) {
            record.text_id = core::TextId{statement->column_int(4)};
        }
        record.provider = statement->column_text(5);
        record.provider_seed = static_cast<std::uint64_t>(statement->column_int(6));
        record.duration = core::Millis{statement->column_int(7)};
        record.graphemes_typed = static_cast<std::size_t>(statement->column_int(8));
        record.graphemes_correct = static_cast<std::size_t>(statement->column_int(9));
        record.errors_total = static_cast<std::size_t>(statement->column_int(10));
        record.errors_uncorrected = static_cast<std::size_t>(statement->column_int(11));
        record.backspaces = static_cast<std::size_t>(statement->column_int(12));
        record.raw_wpm = core::Wpm{statement->column_double(13)};
        record.gross_wpm = core::Wpm{statement->column_double(14)};
        record.net_wpm = core::Wpm{statement->column_double(15)};
        record.accuracy = core::Accuracy{statement->column_double(16)};
        record.final_correctness = core::Accuracy{statement->column_double(17)};
        record.consistency = statement->column_double(18);
        if (!statement->column_is_null(19)) {
            record.peak_wpm = core::Wpm{statement->column_double(19)};
        }
        if (!statement->column_is_null(20)) {
            record.wall_wpm = core::Wpm{statement->column_double(20)};
        }
        record.completed = statement->column_int(21) != 0;
        record.app_version = statement->column_text(22);

        Result<std::vector<core::TimelineSample>> samples = read_samples(id);
        if (!samples) {
            return std::unexpected{samples.error()};
        }
        record.timeline = std::move(*samples);
        return record;
    }

    Result<std::vector<core::TimelineSample>> SqliteHistoryRepository::read_samples(core::SessionId id) const {
        Result<Statement> statement = database_->prepare(
                "SELECT t_ms, wpm, errors, pacer_wpm FROM session_sample"
                " WHERE session_id = ?1 ORDER BY t_ms");
        if (!statement) {
            return std::unexpected{statement.error()};
        }
        statement->bind(1, id.value);

        std::vector<core::TimelineSample> samples;
        for (;;) {
            const Result<bool> row = statement->step();
            if (!row) {
                return std::unexpected{row.error()};
            }
            if (!*row) {
                break;
            }
            // `keystrokes` is not stored — the schema keeps time, speed and
            // errors, which is what the chart draws — so a sample read back is
            // not byte-for-byte the one that was written. Said here rather than
            // left for somebody to discover in a diff.
            core::TimelineSample sample{.at = core::Millis{statement->column_int(0)},
                                        .wpm = core::Wpm{statement->column_double(1)},
                                        .keystrokes = 0,
                                        .errors = static_cast<std::size_t>(statement->column_int(2)),
                                        .pacer_wpm = std::nullopt};
            if (!statement->column_is_null(3)) {
                sample.pacer_wpm = core::Wpm{statement->column_double(3)};
            }
            samples.push_back(sample);
        }
        return samples;
    }

    Result<app::Aggregates> SqliteHistoryRepository::aggregates(const app::HistoryFilter& filter) const {
        // COALESCE, so an empty range is zeros rather than NULLs read as
        // garbage — a new user's history screen is a normal thing to draw.
        Result<Statement> statement = database_->prepare(
                "SELECT COUNT(*), COALESCE(AVG(net_wpm), 0), COALESCE(MAX(net_wpm), 0), COALESCE(MIN(net_wpm), 0),"
                " COALESCE(AVG(accuracy), 0), COALESCE(SUM(duration_ms), 0), COALESCE(SUM(graphemes_typed), 0)"
                " FROM session"
                " WHERE (?1 = '' OR mode = ?1)"
                "   AND (?2 = 0 OR started_at >= ?2)"
                "   AND (?3 = 0 OR started_at < ?3)"
                "   AND (?4 = 0 OR completed = 1)");
        if (!statement) {
            return std::unexpected{statement.error()};
        }

        bind_filter(*statement, bind_values(filter));
        const Result<bool> row = statement->step();
        if (!row) {
            return std::unexpected{row.error()};
        }
        if (!*row) {
            return app::Aggregates{};
        }

        return app::Aggregates{
                .sessions = static_cast<std::size_t>(statement->column_int(0)),
                .mean_net_wpm = core::Wpm{statement->column_double(1)},
                .best_net_wpm = core::Wpm{statement->column_double(2)},
                .worst_net_wpm = core::Wpm{statement->column_double(3)},
                .mean_accuracy = core::Accuracy{statement->column_double(4)},
                .total_time = core::Millis{statement->column_int(5)},
                .total_graphemes = static_cast<std::size_t>(statement->column_int(6)),
        };
    }

    std::string_view SqliteHistoryRepository::daily_totals_sql() noexcept {
        return "SELECT CAST(FLOOR((started_at + ?6) / 86400000.0) AS INTEGER) AS day,"
               " COUNT(*), AVG(net_wpm), AVG(accuracy), SUM(duration_ms)"
               " FROM session"
               " WHERE (?1 = '' OR mode = ?1)"
               "   AND (?2 = 0 OR started_at >= ?2)"
               "   AND (?3 = 0 OR started_at < ?3)"
               "   AND (?4 = 0 OR completed = 1)"
               " GROUP BY day ORDER BY day LIMIT ?5";
    }

    Result<std::vector<app::DayBucket>> SqliteHistoryRepository::daily_totals(const app::HistoryFilter& filter,
                                                                              app::UtcOffsetMinutes offset) const {
        // Grouped by the database, not by loading every row and adding them up
        // in C++ (TI-109). A year is at most 366 rows out of here however many
        // runs are behind them, so the memory a history screen needs stops
        // growing with how much somebody has typed.
        //
        // FLOOR on a real division rather than integer division: SQLite
        // truncates towards zero, which would put New Year's Eve 1969 in 1970.
        // Nobody has a session from 1969, and a boundary that is wrong only for
        // impossible input is still a boundary somebody has to reason about.
        Result<Statement> statement = database_->prepare(daily_totals_sql());
        if (!statement) {
            return std::unexpected{statement.error()};
        }

        bind_filter(*statement, bind_values(filter));
        statement->bind(5, limit_of(filter));
        statement->bind(6, static_cast<std::int64_t>(offset) * 60'000);

        std::vector<app::DayBucket> buckets;
        for (;;) {
            const Result<bool> row = statement->step();
            if (!row) {
                return std::unexpected{row.error()};
            }
            if (!*row) {
                break;
            }
            buckets.push_back(app::DayBucket{
                    .day = statement->column_int(0),
                    .sessions = static_cast<std::size_t>(statement->column_int(1)),
                    .mean_net_wpm = core::Wpm{statement->column_double(2)},
                    .mean_accuracy = core::Accuracy{statement->column_double(3)},
                    .total_time = core::Millis{statement->column_int(4)},
            });
        }
        return buckets;
    }

    Result<std::vector<app::PersonalBest>> SqliteHistoryRepository::personal_bests() const {
        Result<Statement> statement = database_->prepare(
                "SELECT mode, param, metric, session_id, value, achieved_at FROM personal_best"
                " ORDER BY mode, param, metric");
        if (!statement) {
            return std::unexpected{statement.error()};
        }

        std::vector<app::PersonalBest> bests;
        for (;;) {
            const Result<bool> row = statement->step();
            if (!row) {
                return std::unexpected{row.error()};
            }
            if (!*row) {
                break;
            }
            bests.push_back(app::PersonalBest{
                    .mode = statement->column_text(0),
                    .param = statement->column_text(1),
                    .metric = statement->column_text(2),
                    .session_id = core::SessionId{statement->column_int(3)},
                    .value = statement->column_double(4),
                    .achieved_at = core::Millis{statement->column_int(5)},
            });
        }
        return bests;
    }

    Status SqliteHistoryRepository::merge_key_stats(const core::KeyStats& stats) {
        Result<Transaction> transaction = database_->begin();
        if (!transaction) {
            return std::unexpected{transaction.error()};
        }
        if (const Status written = write_key_stats(stats); !written) {
            return written;
        }
        return transaction->commit();
    }

    Status SqliteHistoryRepository::write_key_stats(const core::KeyStats& stats) {
        // Nothing to merge is not a reason to prepare two statements. `save`
        // reaches here with empty totals on every call.
        if (stats.per_grapheme.empty() && stats.per_bigram.empty()) {
            return {};
        }

        Result<Statement> keys = database_->prepare(
                "INSERT INTO key_stat (grapheme, attempts, errors, total_latency_ms) VALUES (?1, ?2, ?3, ?4)"
                " ON CONFLICT (grapheme) DO UPDATE SET"
                "   attempts = key_stat.attempts + excluded.attempts,"
                "   errors = key_stat.errors + excluded.errors,"
                "   total_latency_ms = key_stat.total_latency_ms + excluded.total_latency_ms");
        if (!keys) {
            return std::unexpected{keys.error()};
        }
        for (const auto& [grapheme, stat]: stats.per_grapheme) {
            keys->reset();
            keys->bind(1, grapheme)
                    .bind(2, static_cast<std::int64_t>(stat.attempts))
                    .bind(3, static_cast<std::int64_t>(stat.errors))
                    .bind(4, stat.total_latency.value);
            if (const Status written = keys->run(); !written) {
                return written;
            }
        }

        Result<Statement> bigrams = database_->prepare(
                "INSERT INTO bigram_stat (bigram, attempts, errors, total_latency_ms) VALUES (?1, ?2, ?3, ?4)"
                " ON CONFLICT (bigram) DO UPDATE SET"
                "   attempts = bigram_stat.attempts + excluded.attempts,"
                "   errors = bigram_stat.errors + excluded.errors,"
                "   total_latency_ms = bigram_stat.total_latency_ms + excluded.total_latency_ms");
        if (!bigrams) {
            return std::unexpected{bigrams.error()};
        }
        for (const auto& [bigram, stat]: stats.per_bigram) {
            bigrams->reset();
            bigrams->bind(1, bigram)
                    .bind(2, static_cast<std::int64_t>(stat.attempts))
                    .bind(3, static_cast<std::int64_t>(stat.errors))
                    .bind(4, stat.total_latency.value);
            if (const Status written = bigrams->run(); !written) {
                return written;
            }
        }

        return {};
    }

    Result<core::KeyStats> SqliteHistoryRepository::key_stats(const app::HistoryFilter& /*filter*/) const {
        // Lifetime totals: key_stat and bigram_stat are merged per session and
        // carry no date of their own, so there is nothing to filter by. A
        // per-window breakdown is recoverable from keystroke_blob when it is
        // enabled, and is not worth a column on every row when it is not
        // (TECHNICAL section 5).
        core::KeyStats stats;

        Result<Statement> keys =
                database_->prepare("SELECT grapheme, attempts, errors, total_latency_ms FROM key_stat");
        if (!keys) {
            return std::unexpected{keys.error()};
        }
        for (;;) {
            const Result<bool> row = keys->step();
            if (!row) {
                return std::unexpected{row.error()};
            }
            if (!*row) {
                break;
            }
            stats.per_grapheme[keys->column_text(0)] = core::KeyStat{
                    .attempts = static_cast<std::size_t>(keys->column_int(1)),
                    .errors = static_cast<std::size_t>(keys->column_int(2)),
                    .total_latency = core::Millis{keys->column_int(3)},
                    .latency_samples = static_cast<std::size_t>(keys->column_int(1)),
            };
        }

        Result<Statement> bigrams =
                database_->prepare("SELECT bigram, attempts, errors, total_latency_ms FROM bigram_stat");
        if (!bigrams) {
            return std::unexpected{bigrams.error()};
        }
        for (;;) {
            const Result<bool> row = bigrams->step();
            if (!row) {
                return std::unexpected{row.error()};
            }
            if (!*row) {
                break;
            }
            stats.per_bigram[bigrams->column_text(0)] = core::KeyStat{
                    .attempts = static_cast<std::size_t>(bigrams->column_int(1)),
                    .errors = static_cast<std::size_t>(bigrams->column_int(2)),
                    .total_latency = core::Millis{bigrams->column_int(3)},
                    .latency_samples = static_cast<std::size_t>(bigrams->column_int(1)),
            };
        }

        return stats;
    }

    Status SqliteHistoryRepository::merge_error_map(const core::ErrorMap& errors) {
        Result<Transaction> transaction = database_->begin();
        if (!transaction) {
            return std::unexpected{transaction.error()};
        }
        if (const Status written = write_error_pairs(errors); !written) {
            return written;
        }
        return transaction->commit();
    }

    Status SqliteHistoryRepository::write_error_pairs(const core::ErrorMap& errors) {
        if (errors.substitutions.empty()) {
            return {};
        }

        Result<Statement> upsert = database_->prepare(
                "INSERT INTO error_pair (expected, typed, count) VALUES (?1, ?2, ?3)"
                " ON CONFLICT (expected, typed) DO UPDATE SET count = error_pair.count + excluded.count");
        if (!upsert) {
            return std::unexpected{upsert.error()};
        }

        for (const auto& [pair, count]: errors.substitutions) {
            upsert->reset();
            upsert->bind(1, pair.first).bind(2, pair.second).bind(3, static_cast<std::int64_t>(count));
            if (const Status written = upsert->run(); !written) {
                return written;
            }
        }

        return {};
    }

    Result<core::Wpm> SqliteHistoryRepository::best_sustained_wpm(core::Days window,
                                                                  core::Accuracy min_accuracy) const {
        // Race mode's starting speed (GAMEPLAY section 3.4). In SQL because it
        // is a maximum over a history that may be years long, and because the
        // window boundary is then one comparison rather than a filter applied
        // to every row that crosses the wire.
        Result<Statement> statement = database_->prepare(
                "SELECT COALESCE(MAX(peak_wpm), 0) FROM session"
                " WHERE completed = 1 AND peak_wpm IS NOT NULL"
                "   AND accuracy >= ?3"
                "   AND (?1 = 0 OR started_at >= ?2)");
        if (!statement) {
            return std::unexpected{statement.error()};
        }

        const Result<std::int64_t> now = database_->query_int("SELECT CAST(strftime('%s', 'now') AS INTEGER) * 1000");
        if (!now) {
            return std::unexpected{now.error()};
        }
        const std::int64_t cutoff = *now - (static_cast<std::int64_t>(window.value) * kMillisPerDay);

        statement->bind(1, static_cast<std::int64_t>(window.value)).bind(2, cutoff).bind(3, min_accuracy.value);
        const Result<bool> row = statement->step();
        if (!row) {
            return std::unexpected{row.error()};
        }
        if (!*row) {
            return core::Wpm{0.0};
        }
        return core::Wpm{statement->column_double(0)};
    }

}  // namespace typeit::infra
