#include "typeit/app/services/HistoryService.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

#include "typeit/app/records/History.h"
#include "typeit/core/util/Result.h"
#include "typeit/core/util/Units.h"

namespace typeit::app {
    namespace {

        constexpr std::int64_t kMillisPerMinute = 60'000;
        constexpr std::int64_t kMillisPerDay = 86'400'000;

        /// Which local day an instant falls in, as a day number. Negative for
        /// anything before 1970, which is why this is a floor division and not
        /// a truncating one — `-1 / 86400000` is 0, and that would put New
        /// Year's Eve 1969 in 1970.
        std::int64_t local_day(core::Millis at, UtcOffsetMinutes offset) {
            const std::int64_t local = at.value + (static_cast<std::int64_t>(offset) * kMillisPerMinute);
            return local >= 0 ? local / kMillisPerDay : ((local + 1) / kMillisPerDay) - 1;
        }

        /// 1 January 1970 was a Thursday, so day 0 is a Thursday and Monday is
        /// three days later.
        std::int64_t local_week(std::int64_t day) {
            const std::int64_t since_monday = (((day + 3) % 7) + 7) % 7;
            return day - since_monday;
        }

        core::Millis midnight_of(std::int64_t day, UtcOffsetMinutes offset) {
            return core::Millis{(day * kMillisPerDay) - (static_cast<std::int64_t>(offset) * kMillisPerMinute)};
        }

        bool needs_quoting(std::string_view field) { return field.find_first_of(",\"\n\r") != std::string_view::npos; }

        void append_csv_field(std::string& out, std::string_view field) {
            if (!needs_quoting(field)) {
                out += field;
                return;
            }
            out += '"';
            for (const char byte: field) {
                if (byte == '"') {
                    out += '"';  // RFC 4180 escapes a quote by doubling it.
                }
                out += byte;
            }
            out += '"';
        }

        void append_json_string(std::string& out, std::string_view value) {
            out += '"';
            for (const char byte: value) {
                switch (byte) {
                    case '"':
                        out += "\\\"";
                        break;
                    case '\\':
                        out += "\\\\";
                        break;
                    case '\n':
                        out += "\\n";
                        break;
                    case '\r':
                        out += "\\r";
                        break;
                    case '\t':
                        out += "\\t";
                        break;
                    default:
                        if (static_cast<unsigned char>(byte) < 0x20) {
                            // The control characters JSON has no short escape
                            // for. Everything above them, UTF-8 included,
                            // passes through as itself.
                            constexpr std::string_view kHex = "0123456789abcdef";
                            out += "\\u00";
                            out += kHex.at((static_cast<unsigned char>(byte) >> 4U) & 0xFU);
                            out += kHex.at(static_cast<unsigned char>(byte) & 0xFU);
                        } else {
                            out += byte;
                        }
                        break;
                }
            }
            out += '"';
        }

        /// Shortest round-trippable form, and never `nan` or `inf`, which are
        /// not JSON and which no spreadsheet reads either. The metrics cannot
        /// produce them; a column that could would be a column that breaks
        /// every consumer of the export.
        std::string number(double value) {
            if (!(value == value) || value > 1e308 || value < -1e308) {
                return "0";
            }
            std::string text = std::to_string(value);
            // std::to_string gives six decimals; trim the ones that say nothing.
            const std::size_t last = text.find_last_not_of('0');
            if (text.contains('.') && last != std::string::npos) {
                text.erase(text.at(last) == '.' ? last : last + 1);
            }
            return text;
        }

    }  // namespace

    core::Result<std::vector<TrendPoint>> HistoryService::trend(const HistoryFilter& filter, TrendBucket bucket,
                                                                UtcOffsetMinutes offset) const {
        const core::Result<std::vector<SessionRow>> rows = history_->query(filter);
        if (!rows) {
            return std::unexpected{rows.error()};
        }

        // Accumulated by bucket, then sorted: the repository answers newest
        // first and a trend line reads oldest first.
        std::vector<std::pair<std::int64_t, TrendPoint>> buckets;
        for (const SessionRow& row: *rows) {
            const std::int64_t day = local_day(row.started_at, offset);
            const std::int64_t key = bucket == TrendBucket::Week ? local_week(day) : day;

            const auto found = std::ranges::find(buckets, key, &std::pair<std::int64_t, TrendPoint>::first);
            TrendPoint& point = found != buckets.end() ? found->second : buckets.emplace_back(key, TrendPoint{}).second;
            point.start = midnight_of(key, offset);
            ++point.sessions;
            point.mean_net_wpm = core::Wpm{point.mean_net_wpm.value + row.net_wpm.value};
            point.mean_accuracy = core::Accuracy{point.mean_accuracy.value + row.accuracy.value};
            point.total_time += row.duration;
        }

        std::ranges::sort(buckets, {}, &std::pair<std::int64_t, TrendPoint>::first);

        std::vector<TrendPoint> points;
        points.reserve(buckets.size());
        for (auto& [key, point]: buckets) {
            const auto count = static_cast<double>(point.sessions);
            point.mean_net_wpm = core::Wpm{point.mean_net_wpm.value / count};
            point.mean_accuracy = core::Accuracy{point.mean_accuracy.value / count};
            points.push_back(point);
        }
        return points;
    }

    core::Result<Streak> HistoryService::streak(const HistoryFilter& filter, core::Millis today,
                                                UtcOffsetMinutes offset) const {
        const core::Result<std::vector<SessionRow>> rows = history_->query(filter);
        if (!rows) {
            return std::unexpected{rows.error()};
        }

        std::vector<std::int64_t> days;
        days.reserve(rows->size());
        for (const SessionRow& row: *rows) {
            days.push_back(local_day(row.started_at, offset));
        }
        std::ranges::sort(days);
        const auto duplicates = std::ranges::unique(days);
        days.erase(duplicates.begin(), duplicates.end());
        if (days.empty()) {
            return Streak{};
        }

        std::size_t longest = 1;
        std::size_t running = 1;
        for (std::size_t i = 1; i < days.size(); ++i) {
            running = days.at(i) == days.at(i - 1) + 1 ? running + 1 : 1;
            longest = std::max(longest, running);
        }

        // The run of days ending at the last day typed only counts as current
        // if that day is today or yesterday. Anything older is a streak that
        // has already been broken, and reporting it as current would tell
        // somebody they are on day nine when they stopped a week ago.
        const std::int64_t last = days.back();
        const std::int64_t now = local_day(today, offset);
        const std::size_t current = (now - last) <= 1 ? running : 0;
        return Streak{.current = current, .longest = longest};
    }

    core::Result<std::string> HistoryService::to_csv(const HistoryFilter& filter) const {
        const core::Result<std::vector<SessionRow>> rows = history_->query(filter);
        if (!rows) {
            return std::unexpected{rows.error()};
        }

        std::string out =
                "id,started_at,mode,mode_param,duration_ms,net_wpm,gross_wpm,accuracy,consistency,completed\n";
        for (const SessionRow& row: *rows) {
            out += std::to_string(row.id.value);
            out += ',';
            out += std::to_string(row.started_at.value);
            out += ',';
            append_csv_field(out, row.mode);
            out += ',';
            append_csv_field(out, row.mode_param);
            out += ',';
            out += std::to_string(row.duration.value);
            out += ',';
            out += number(row.net_wpm.value);
            out += ',';
            out += number(row.gross_wpm.value);
            out += ',';
            out += number(row.accuracy.value);
            out += ',';
            out += number(row.consistency);
            out += ',';
            out += row.completed ? "1" : "0";
            out += '\n';
        }
        return out;
    }

    core::Result<std::string> HistoryService::to_json(const HistoryFilter& filter) const {
        const core::Result<std::vector<SessionRow>> rows = history_->query(filter);
        if (!rows) {
            return std::unexpected{rows.error()};
        }

        std::string out = "[";
        bool first = true;
        for (const SessionRow& row: *rows) {
            if (!first) {
                out += ',';
            }
            first = false;
            out += R"({"id":)" + std::to_string(row.id.value);
            out += R"(,"started_at":)" + std::to_string(row.started_at.value);
            out += R"(,"mode":)";
            append_json_string(out, row.mode);
            out += R"(,"mode_param":)";
            append_json_string(out, row.mode_param);
            out += R"(,"duration_ms":)" + std::to_string(row.duration.value);
            out += R"(,"net_wpm":)" + number(row.net_wpm.value);
            out += R"(,"gross_wpm":)" + number(row.gross_wpm.value);
            out += R"(,"accuracy":)" + number(row.accuracy.value);
            out += R"(,"consistency":)" + number(row.consistency);
            out += R"(,"completed":)";
            out += row.completed ? "true" : "false";
            out += '}';
        }
        out += ']';
        return out;
    }

}  // namespace typeit::app
