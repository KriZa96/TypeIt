// In-memory implementations of every port (TI-064).
//
// Phase 3's service tests run against these: a service is orchestration, and
// orchestration is testable without a database if the thing standing in for one
// behaves like it. That "if" is the whole risk of a fake, and it is why every
// one of these is run through the same contract suite as the real adapter
// (TI-063) rather than trusted to be equivalent.
//
// Each can be made to fail on demand, because an error path nobody can produce
// is an error path nobody has tested.
#ifndef TYPEIT_TESTING_FAKES_H
#define TYPEIT_TESTING_FAKES_H

#include <algorithm>
#include <cctype>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <map>
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <tuple>
#include <utility>
#include <vector>

#include "typeit/app/ports/Ports.h"
#include "typeit/app/records/History.h"
#include "typeit/app/records/TextLibrary.h"
#include "typeit/core/config/Config.h"
#include "typeit/core/metrics/ErrorMap.h"
#include "typeit/core/metrics/KeyStats.h"
#include "typeit/core/util/Result.h"
#include "typeit/core/util/Units.h"

namespace typeit::testing {

    /// One armed failure. Consumed by the next call that checks it, so a test
    /// says "the next save fails" rather than "everything fails from now on" —
    /// the first is a scenario, the second is a different object.
    class FailureSwitch {
    public:
        void fail_next(core::Error error) { next_ = std::move(error); }

        [[nodiscard]] bool armed() const noexcept { return next_.has_value(); }

        /// The armed error, if any, and disarms.
        [[nodiscard]] std::optional<core::Error> take() {
            std::optional<core::Error> error = std::move(next_);
            next_.reset();
            return error;
        }

    private:
        std::optional<core::Error> next_;
    };

/// Every fake method starts with this: if a failure is armed, that is what
/// happens instead.
#define TYPEIT_FAIL_IF_ARMED()                                                  \
    if (std::optional<core::Error> armed = failure.take(); armed.has_value()) { \
        return std::unexpected{*armed};                                         \
    }

    class FakeHistoryRepository final : public app::IHistoryRepository {
    public:
        [[nodiscard]] core::Result<core::SessionId> save(const app::SessionRecord& record) override {
            TYPEIT_FAIL_IF_ARMED()
            return write_run(record, {}, {});
        }

        [[nodiscard]] core::Result<core::SessionId> save_run(const app::SessionRecord& record,
                                                             const core::KeyStats& stats,
                                                             const core::ErrorMap& map) override {
            TYPEIT_FAIL_IF_ARMED()
            return write_run(record, stats, map);
        }

        [[nodiscard]] core::Result<std::vector<app::SessionRow>> query(
                const app::HistoryFilter& filter) const override {
            TYPEIT_FAIL_IF_ARMED()
            ++queries;

            std::vector<app::SessionRow> matched;
            for (const app::SessionRow& row: rows) {
                if (matches(row, filter)) {
                    matched.push_back(row);
                }
            }
            // Newest first, and the id breaks a tie — the same order the SQL
            // asks for, because the contract suite compares them.
            std::ranges::sort(matched, [](const app::SessionRow& left, const app::SessionRow& right) {
                if (left.started_at != right.started_at) {
                    return left.started_at > right.started_at;
                }
                return left.id > right.id;
            });
            if (filter.limit != 0 && matched.size() > filter.limit) {
                matched.resize(filter.limit);
            }
            return matched;
        }

        [[nodiscard]] core::Result<app::Aggregates> aggregates(const app::HistoryFilter& filter) const override {
            TYPEIT_FAIL_IF_ARMED()

            app::Aggregates totals;
            double net_sum = 0.0;
            double accuracy_sum = 0.0;
            for (std::size_t i = 0; i < rows.size(); ++i) {
                if (!matches(rows[i], filter)) {
                    continue;
                }
                const app::SessionRecord& record = records[i];
                if (totals.sessions == 0) {
                    totals.best_net_wpm = record.net_wpm;
                    totals.worst_net_wpm = record.net_wpm;
                }
                ++totals.sessions;
                net_sum += record.net_wpm.value;
                accuracy_sum += record.accuracy.value;
                totals.best_net_wpm = core::Wpm{std::max(totals.best_net_wpm.value, record.net_wpm.value)};
                totals.worst_net_wpm = core::Wpm{std::min(totals.worst_net_wpm.value, record.net_wpm.value)};
                totals.total_time += record.duration;
                totals.total_graphemes += record.graphemes_typed;
            }
            if (totals.sessions > 0) {
                const auto count = static_cast<double>(totals.sessions);
                totals.mean_net_wpm = core::Wpm{net_sum / count};
                totals.mean_accuracy = core::Accuracy{accuracy_sum / count};
            }
            return totals;
        }

        [[nodiscard]] core::Result<std::vector<app::PersonalBest>> personal_bests() const override {
            TYPEIT_FAIL_IF_ARMED()

            std::vector<app::PersonalBest> all;
            all.reserve(bests_.size());
            for (const auto& [key, best]: bests_) {
                all.push_back(best);
            }
            return all;
        }

        [[nodiscard]] core::Status merge_key_stats(const core::KeyStats& stats) override {
            TYPEIT_FAIL_IF_ARMED()
            ++merges;
            merge_into(stats);
            return {};
        }

        [[nodiscard]] core::Result<core::KeyStats> key_stats(const app::HistoryFilter& /*filter*/) const override {
            TYPEIT_FAIL_IF_ARMED()
            return keys;
        }

        [[nodiscard]] core::Status merge_error_map(const core::ErrorMap& map) override {
            TYPEIT_FAIL_IF_ARMED()
            merge_into(map);
            return {};
        }

        [[nodiscard]] core::Result<core::Wpm> best_sustained_wpm(core::Days window) const override {
            TYPEIT_FAIL_IF_ARMED()

            const core::Millis cutoff{now.value - (static_cast<std::int64_t>(window.value) * 86'400'000)};
            double best = 0.0;
            for (const app::SessionRecord& record: records) {
                if (!record.completed || !record.peak_wpm.has_value()) {
                    continue;
                }
                if (window.value != 0 && record.started_at < cutoff) {
                    continue;
                }
                best = std::max(best, record.peak_wpm->value);
            }
            return core::Wpm{best};
        }

        /// The clock the window is measured against. A fake that asked the real
        /// one would make every window test depend on the day it runs.
        core::Millis now{1'767'225'600'000};

        mutable FailureSwitch failure;
        std::vector<app::SessionRecord> records;
        std::vector<app::SessionRow> rows;
        core::KeyStats keys;
        core::ErrorMap errors;
        std::size_t saves = 0;
        std::size_t merges = 0;
        /// How many times anybody asked for rows. `mutable` because `query` is
        /// const and counting the calls is the only way to assert that drawing
        /// a screen does not make any — which is the whole of TI-103's
        /// "nothing is queried in render".
        mutable std::size_t queries = 0;

    private:
        /// Applies every write of a run, or none of them.
        ///
        /// Atomicity here is not a formality copied from the SQL: the sample
        /// primary key is `(session_id, t_ms)`, so a timeline with a repeated
        /// timestamp fails the real adapter **after** the session row is
        /// written. A fake that accepted it would let a service test pass over
        /// a database that would have rolled the run back.
        [[nodiscard]] core::Result<core::SessionId> write_run(const app::SessionRecord& record,
                                                              const core::KeyStats& stats, const core::ErrorMap& map) {
            std::set<std::int64_t> seen;
            for (const core::TimelineSample& sample: record.timeline) {
                if (!seen.insert(sample.at.value).second) {
                    return core::fail(core::ErrorCode::DbQuery, "UNIQUE constraint failed: session_sample.t_ms");
                }
            }

            ++saves;
            const core::SessionId id{next_id_++};
            records.push_back(record);
            rows.push_back(app::SessionRow{
                    .id = id,
                    .started_at = record.started_at,
                    .mode = record.mode,
                    .mode_param = record.mode_param,
                    .duration = record.duration,
                    .net_wpm = record.net_wpm,
                    .gross_wpm = record.gross_wpm,
                    .accuracy = record.accuracy,
                    .consistency = record.consistency,
                    .completed = record.completed,
            });
            update_bests(id, record);
            merge_into(stats);
            merge_into(map);
            return id;
        }

        void merge_into(const core::KeyStats& stats) {
            for (const auto& [grapheme, stat]: stats.per_grapheme) {
                add_to(keys.per_grapheme[grapheme], stat);
            }
            for (const auto& [bigram, stat]: stats.per_bigram) {
                add_to(keys.per_bigram[bigram], stat);
            }
        }

        void merge_into(const core::ErrorMap& map) {
            for (const auto& [pair, count]: map.substitutions) {
                errors.substitutions[pair] += count;
            }
        }

        static void add_to(core::KeyStat& into, const core::KeyStat& from) {
            into.attempts += from.attempts;
            into.errors += from.errors;
            into.total_latency += from.total_latency;
            into.latency_samples += from.latency_samples;
        }

        static bool matches(const app::SessionRow& row, const app::HistoryFilter& filter) {
            if (filter.mode.has_value() && row.mode != *filter.mode) {
                return false;
            }
            if (filter.since.has_value() && row.started_at < *filter.since) {
                return false;
            }
            if (filter.until.has_value() && !(row.started_at < *filter.until)) {
                return false;
            }
            return !(filter.completed_only && !row.completed);
        }

        void update_bests(core::SessionId id, const app::SessionRecord& record) {
            // GAMEPLAY section 7.3, and the same rule the SQL enforces: an
            // abandoned or inaccurate run never qualifies, and an equal one
            // leaves the older record standing.
            if (!record.completed || record.accuracy.value < 0.90) {
                return;
            }
            consider(id, record, "net_wpm", record.net_wpm.value);
            consider(id, record, "accuracy", record.accuracy.value);
            if (record.peak_wpm.has_value()) {
                consider(id, record, "peak_wpm", record.peak_wpm->value);
            }
        }

        void consider(core::SessionId id, const app::SessionRecord& record, std::string metric, double value) {
            const auto key = std::tuple{record.mode, record.mode_param, metric};
            const auto existing = bests_.find(key);
            if (existing != bests_.end() && !(value > existing->second.value)) {
                return;
            }
            bests_[key] = app::PersonalBest{
                    .mode = record.mode,
                    .param = record.mode_param,
                    .metric = std::move(metric),
                    .session_id = id,
                    .value = value,
                    .achieved_at = record.ended_at,
            };
        }

        std::map<std::tuple<std::string, std::string, std::string>, app::PersonalBest> bests_;
        std::int64_t next_id_ = 1;
    };

    class FakeTextLibraryRepository final : public app::ITextLibraryRepository {
    public:
        [[nodiscard]] core::Result<core::TextId> add(const app::TextItem& text) override {
            TYPEIT_FAIL_IF_ARMED()

            for (const app::TextItem& existing: texts) {
                if (existing.content_sha256 == text.content_sha256) {
                    return core::fail(core::ErrorCode::DbQuery, "UNIQUE constraint failed: text_item.content_sha256");
                }
            }
            app::TextItem stored = text;
            stored.id = core::TextId{next_id_++};
            texts.push_back(stored);
            ++adds;
            return stored.id;
        }

        [[nodiscard]] core::Result<std::vector<app::TextSummary>> list(const app::TextFilter& filter) const override {
            TYPEIT_FAIL_IF_ARMED()

            std::vector<app::TextSummary> matched;
            for (const app::TextItem& text: texts) {
                if (!matches(text, filter)) {
                    continue;
                }
                app::TextSummary summary;
                summary.id = text.id;
                summary.title = text.title;
                summary.source = text.source;
                summary.origin = text.origin;
                summary.grapheme_count = text.grapheme_count;
                summary.word_count = text.word_count;
                summary.difficulty = text.difficulty;
                summary.created_at = text.created_at;
                summary.tags = tags_of(text.id);
                matched.push_back(std::move(summary));
            }
            std::ranges::sort(matched, [](const app::TextSummary& left, const app::TextSummary& right) {
                if (left.created_at != right.created_at) {
                    return left.created_at > right.created_at;
                }
                return left.id > right.id;
            });
            if (filter.limit != 0 && matched.size() > filter.limit) {
                matched.resize(filter.limit);
            }
            return matched;
        }

        [[nodiscard]] core::Result<std::optional<app::TextItem>> get(core::TextId id) const override {
            TYPEIT_FAIL_IF_ARMED()

            for (const app::TextItem& text: texts) {
                if (text.id == id) {
                    return text;
                }
            }
            return std::optional<app::TextItem>{};
        }

        [[nodiscard]] core::Result<std::optional<app::TextItem>> find_by_hash(std::string_view sha256) const override {
            TYPEIT_FAIL_IF_ARMED()

            for (const app::TextItem& text: texts) {
                if (text.content_sha256 == sha256) {
                    return text;
                }
            }
            return std::optional<app::TextItem>{};
        }

        [[nodiscard]] core::Status remove(core::TextId id) override {
            TYPEIT_FAIL_IF_ARMED()

            std::erase_if(texts, [id](const app::TextItem& text) { return text.id == id; });
            std::erase_if(tags, [id](const auto& entry) { return entry.first == id.value; });
            bookmarks.erase(id.value);
            return {};
        }

        [[nodiscard]] core::Status tag(core::TextId id, std::string_view name) override {
            TYPEIT_FAIL_IF_ARMED()

            std::vector<std::string>& applied = tags[id.value];
            if (std::ranges::find(applied, name) == applied.end()) {
                applied.emplace_back(name);
            }
            return {};
        }

        [[nodiscard]] core::Status untag(core::TextId id, std::string_view name) override {
            TYPEIT_FAIL_IF_ARMED()

            std::erase(tags[id.value], std::string{name});
            return {};
        }

        [[nodiscard]] core::Status set_bookmark(const app::Bookmark& bookmark) override {
            TYPEIT_FAIL_IF_ARMED()

            bookmarks[bookmark.text_id.value] = bookmark;
            return {};
        }

        [[nodiscard]] core::Result<std::optional<app::Bookmark>> bookmark(core::TextId id) const override {
            TYPEIT_FAIL_IF_ARMED()

            const auto found = bookmarks.find(id.value);
            if (found == bookmarks.end()) {
                return std::optional<app::Bookmark>{};
            }
            return found->second;
        }

        mutable FailureSwitch failure;
        std::vector<app::TextItem> texts;
        std::map<std::int64_t, std::vector<std::string>> tags;
        std::map<std::int64_t, app::Bookmark> bookmarks;
        /// How many rows were written. Distinct from `texts.size()`: a service
        /// that recognises a duplicate must not have called `add` at all.
        std::size_t adds = 0;

    private:
        [[nodiscard]] std::vector<std::string> tags_of(core::TextId id) const {
            const auto found = tags.find(id.value);
            if (found == tags.end()) {
                return {};
            }
            std::vector<std::string> applied = found->second;
            std::ranges::sort(applied);
            return applied;
        }

        [[nodiscard]] bool matches(const app::TextItem& text, const app::TextFilter& filter) const {
            if (filter.search.has_value()) {
                // ASCII case folding, like SQLite's LIKE — the real adapter can
                // do no better without ICU, so the fake must do no better
                // either.
                std::string title = text.title;
                std::string term = *filter.search;
                std::ranges::transform(title, title.begin(), [](unsigned char c) { return std::tolower(c); });
                std::ranges::transform(term, term.begin(), [](unsigned char c) { return std::tolower(c); });
                if (title.find(term) == std::string::npos) {
                    return false;
                }
            }
            const std::vector<std::string> applied = tags_of(text.id);
            for (const std::string& wanted: filter.tags) {
                if (std::ranges::find(applied, wanted) == applied.end()) {
                    return false;
                }
            }
            return true;
        }

        std::int64_t next_id_ = 1;
    };

    class FakeConfigStore final : public app::IConfigStore {
    public:
        [[nodiscard]] core::Result<app::LoadedConfig> load() override {
            TYPEIT_FAIL_IF_ARMED()
            ++loads;
            return loaded;
        }

        [[nodiscard]] core::Status save(const core::Config& config) override {
            TYPEIT_FAIL_IF_ARMED()
            ++saves;
            loaded.config = config;
            return {};
        }

        mutable FailureSwitch failure;
        app::LoadedConfig loaded;
        std::size_t loads = 0;
        std::size_t saves = 0;
    };

    class FakeFileSystem final : public app::IFileSystem {
    public:
        [[nodiscard]] core::Result<std::string> read_text(const std::filesystem::path& path) const override {
            TYPEIT_FAIL_IF_ARMED()

            const auto found = files.find(path.generic_string());
            if (found == files.end()) {
                if (directories.contains(path.generic_string())) {
                    return core::fail(core::ErrorCode::FileUnreadable, path.string() + ": is a directory");
                }
                return core::fail(core::ErrorCode::FileNotFound, path.string());
            }
            return found->second;
        }

        [[nodiscard]] bool exists(const std::filesystem::path& path) const override {
            return files.contains(path.generic_string()) || directories.contains(path.generic_string());
        }

        [[nodiscard]] bool is_directory(const std::filesystem::path& path) const override {
            return directories.contains(path.generic_string());
        }

        [[nodiscard]] core::Result<std::vector<std::filesystem::path>> list(
                const std::filesystem::path& directory) const override {
            TYPEIT_FAIL_IF_ARMED()

            if (!directories.contains(directory.generic_string())) {
                return core::fail(core::ErrorCode::FileNotFound, directory.string() + ": not a directory");
            }

            std::vector<std::filesystem::path> entries;
            for (const auto& [path, contents]: files) {
                if (std::filesystem::path{path}.parent_path() == directory) {
                    entries.emplace_back(path);
                }
            }
            for (const std::string& path: directories) {
                if (std::filesystem::path{path}.parent_path() == directory) {
                    entries.emplace_back(path);
                }
            }
            std::ranges::sort(entries);
            return entries;
        }

        /// Adds a file and every directory above it, so a test writes one line
        /// rather than building a tree.
        void add_file(const std::filesystem::path& path, std::string contents) {
            files[path.generic_string()] = std::move(contents);
            for (std::filesystem::path parent = path.parent_path(); !parent.empty() && parent != parent.root_path();
                 parent = parent.parent_path()) {
                directories.insert(parent.generic_string());
            }
        }

        void add_directory(const std::filesystem::path& path) { directories.insert(path.generic_string()); }

        mutable FailureSwitch failure;
        std::map<std::string, std::string> files;
        std::set<std::string> directories;
    };

    class FakeAssetLocator final : public app::IAssetLocator {
    public:
        [[nodiscard]] core::Result<std::filesystem::path> locate(std::string_view kind) const override {
            TYPEIT_FAIL_IF_ARMED()

            const auto found = kinds.find(std::string{kind});
            if (found == kinds.end()) {
                return core::fail(core::ErrorCode::FileNotFound, std::string{kind} + ": no such assets");
            }
            return found->second;
        }

        mutable FailureSwitch failure;
        std::map<std::string, std::filesystem::path> kinds;
    };

#undef TYPEIT_FAIL_IF_ARMED

}  // namespace typeit::testing

#endif  // TYPEIT_TESTING_FAKES_H
