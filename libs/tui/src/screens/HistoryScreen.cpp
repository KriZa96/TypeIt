#include "screens/HistoryScreen.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <ftxui/component/event.hpp>
#include <ftxui/dom/elements.hpp>
#include <string>
#include <utility>
#include <vector>

#include "Bars.h"
#include "Charts.h"
#include "ColorQuantizer.h"
#include "Heatmap.h"
#include "Keymap.h"
#include "typeit/app/Json.h"
#include "typeit/core/config/Validation.h"

namespace typeit::tui {
    namespace {

        constexpr std::int64_t kMillisPerDay = 86'400'000;
        constexpr std::int64_t kMillisPerMinute = 60'000;

        /// Everything above the session list, counted rather than guessed:
        /// title, blank, chart, blank, totals, today, bests, blank, the
        /// heatmap's caption and its four rows, blank, "Recent" — and below it
        /// a blank and the hint bar. A `kChromeRows` that was smaller than the
        /// truth promised rows the screen never drew and pushed the hint bar
        /// off the bottom of an 80x24 terminal.
        constexpr std::size_t kChartRows = 7;
        constexpr std::size_t kShortChartRows = 5;
        constexpr std::size_t kHeatmapRows = 6;
        constexpr std::size_t kChromeRows = 10;
        constexpr std::size_t kMinimumRows = 1;

        /// Below this the heatmap goes and the chart shrinks. A keyboard
        /// diagram is the least urgent thing here, and losing the key hints
        /// and half the list to keep it is the wrong trade.
        constexpr std::size_t kRoomForEverything = 30;

        /// How many records fit on the one line they get. More than this and
        /// the line wraps, which is worse than a shorter list.
        constexpr std::size_t kBestsShown = 4;

        /// `all` plus every mode a run can be recorded under, so the filter
        /// offers what the history can contain rather than what it happens to.
        const std::vector<std::string>& mode_choices() {
            static const std::vector<std::string> choices = [] {
                std::vector<std::string> all{"all"};
                for (const std::string_view mode: core::kModeNames) {
                    all.emplace_back(mode);
                }
                return all;
            }();
            return choices;
        }

        std::string duration_text(core::Millis total) {
            const std::int64_t minutes = total.value / kMillisPerMinute;
            return std::to_string(minutes / 60) + "h " + std::to_string(minutes % 60) + "m";
        }

        std::string whole(double value) { return std::to_string(static_cast<std::int64_t>(value)); }

        /// "1 run", "2 runs". Worth the three lines: a history screen is read
        /// often enough that "1 runs" becomes the thing somebody notices.
        std::string runs_text(std::size_t count) { return std::to_string(count) + (count == 1 ? " run" : " runs"); }

    }  // namespace

    std::string_view to_string(DateRange range) {
        switch (range) {
            case DateRange::All:
                return "all time";
            case DateRange::Week:
                return "7 days";
            case DateRange::Month:
                return "30 days";
            case DateRange::Year:
                return "365 days";
        }
        return "all time";
    }

    std::int64_t days_in(DateRange range) {
        switch (range) {
            case DateRange::All:
                return 0;
            case DateRange::Week:
                return 7;
            case DateRange::Month:
                return 30;
            case DateRange::Year:
                return 365;
        }
        return 0;
    }

    HistoryScreen::HistoryScreen(const ScreenContext& context) : context_{&context} {
        // Abandoned runs are in: this screen is a record of what happened, and
        // a run left half way through happened. The trend excludes them by
        // asking for its own filter.
        filter_.completed_only = false;
        reload();
    }

    void HistoryScreen::reload() {
        data_ = {};
        const HistorySource& source = context_->history;
        if (source.records == nullptr) {
            // No history to read. Not an error and not a message — the empty
            // state below is what a new user sees, and it is the same one.
            return;
        }

        const auto note = [&](const core::Error& error) { data_.problems.push_back(core::to_string(error)); };

        if (core::Result<std::vector<app::SessionRow>> rows = source.records->query(filter_); rows) {
            data_.sessions = std::move(*rows);
        } else {
            note(rows.error());
        }
        if (const core::Result<app::Aggregates> totals = source.records->aggregates(filter_); totals) {
            data_.totals = *totals;
        } else {
            note(totals.error());
        }
        if (const core::Result<std::vector<app::PersonalBest>> bests = source.records->personal_bests(); bests) {
            data_.bests = *bests;
        } else {
            note(bests.error());
        }
        if (core::Result<core::KeyStats> keys = source.records->key_stats(filter_); keys) {
            data_.keys = std::move(*keys);
        } else {
            note(keys.error());
        }

        if (source.service != nullptr) {
            if (core::Result<std::vector<app::TrendPoint>> trend =
                        source.service->trend(filter_, app::TrendBucket::Day, source.utc_offset);
                trend) {
                data_.trend = std::move(*trend);
            } else {
                note(trend.error());
            }
            if (source.wall_clock != nullptr) {
                // The configured goal, so the streak on screen means the same
                // thing as the one in `--stats` and as GAMEPLAY §7.4 says.
                const app::DailyGoal goal{
                        .time = core::Millis{context_->config->goals.daily_minutes * kMillisPerMinute},
                        .runs = static_cast<std::size_t>(std::max<std::int64_t>(0, context_->config->goals.daily_runs)),
                };
                const core::Millis now = source.wall_clock->unix_now();
                if (const core::Result<app::Streak> streak =
                            source.service->streak(filter_, now, source.utc_offset, goal);
                    streak) {
                    data_.streak = *streak;
                } else {
                    note(streak.error());
                }
                if (const core::Result<app::GoalProgress> today =
                            source.service->today(filter_, now, source.utc_offset, goal);
                    today) {
                    data_.today = *today;
                } else {
                    note(today.error());
                }
            }
        }

        selected_ = 0;
        first_row_ = 0;
    }

    bool HistoryScreen::roomy() const { return context_->size.rows >= kRoomForEverything; }

    std::size_t HistoryScreen::page_size() const {
        const std::size_t chrome = kChromeRows + (roomy() ? kChartRows + kHeatmapRows : kShortChartRows);
        const std::size_t rows = context_->size.rows;
        return rows > chrome ? rows - chrome : kMinimumRows;
    }

    void HistoryScreen::cycle_mode(bool forward) {
        const std::vector<std::string>& choices = mode_choices();
        const std::string current = filter_.mode.value_or("all");
        // NOLINTNEXTLINE(readability-qualified-auto) -- MSVC's vector iterator is not a pointer
        const auto at = std::ranges::find(choices, current);
        const std::size_t index = at == choices.end() ? 0 : static_cast<std::size_t>(at - choices.begin());
        const std::size_t count = choices.size();
        const std::string& next = choices.at(forward ? (index + 1) % count : (index + count - 1) % count);

        // `all` is the absence of a filter, not a mode called "all" — a query
        // for `mode = 'all'` matches nothing.
        filter_.mode = next == "all" ? std::optional<std::string>{} : std::optional<std::string>{next};
        reload();
    }

    void HistoryScreen::cycle_range(bool forward) {
        const auto index = static_cast<std::size_t>(range_);
        const std::size_t count = kAllDateRanges.size();
        range_ = kAllDateRanges.at(forward ? (index + 1) % count : (index + count - 1) % count);

        // Combinable with the mode filter by construction: this touches `since`
        // and nothing else.
        const std::int64_t days = days_in(range_);
        if (days == 0 || context_->history.wall_clock == nullptr) {
            filter_.since.reset();
        } else {
            filter_.since = core::Millis{context_->history.wall_clock->unix_now().value - (days * kMillisPerDay)};
        }
        reload();
    }

    void HistoryScreen::move_selection(std::int64_t by) {
        if (data_.sessions.empty()) {
            return;
        }
        const auto last = static_cast<std::int64_t>(data_.sessions.size() - 1);
        const auto wanted = std::clamp(static_cast<std::int64_t>(selected_) + by, std::int64_t{0}, last);
        selected_ = static_cast<std::size_t>(wanted);

        // The window follows the selection rather than the other way round, so
        // paging past the end lands on the last row instead of scrolling into
        // blank space.
        const std::size_t page = page_size();
        if (selected_ < first_row_) {
            first_row_ = selected_;
        } else if (selected_ >= first_row_ + page) {
            first_row_ = selected_ - page + 1;
        }
    }

    void HistoryScreen::write_export() {
        const HistorySource& source = context_->history;
        if (source.service == nullptr || !context_->save_text) {
            export_message_ = "exporting is not available in this build";
            return;
        }
        if (export_path_.empty()) {
            export_message_ = "type a path to write to";
            return;
        }

        // The extension chooses the format. Predictable, and it means the two
        // exports are reachable without a second control nobody would find.
        const std::filesystem::path path{export_path_};
        const bool json = path.extension() == ".json";
        // The *same* filter the screen is showing, so what lands in the file is
        // what the user was looking at rather than the whole history.
        const core::Result<std::string> text =
                json ? source.service->to_json(filter_) : source.service->to_csv(filter_);
        if (!text) {
            export_message_ = core::to_string(text.error());
            return;
        }

        if (const core::Status written = context_->save_text(path, *text); !written) {
            // Named, not swallowed: an unwritable path is the common failure
            // and the one a silent export hides completely.
            export_message_ = core::to_string(written.error());
            return;
        }
        export_message_ = "wrote " + std::to_string(data_.sessions.size()) + " runs to " + export_path_;
        exporting_ = false;
    }

    std::optional<core::SessionId> HistoryScreen::take_opened() {
        std::optional<core::SessionId> taken = opened_;
        opened_.reset();
        return taken;
    }

    namespace {

        ftxui::Element bests_line(const std::vector<app::PersonalBest>& bests, const Styling& muted,
                                  const Styling& accent) {
            if (bests.empty()) {
                return ftxui::text("  no records yet") | ftxui::color(muted.color);
            }
            std::vector<ftxui::Element> parts{ftxui::text("  bests   ") | ftxui::color(muted.color)};
            // Per `(mode, parameter, metric)`: a 15-second best and a
            // 60-second best are separate records because they measure
            // different things, and a WPM record and an accuracy record are not
            // comparable at all — so the metric is named rather than left for
            // the reader to infer from the magnitude.
            for (std::size_t at = 0; at < bests.size() && at < kBestsShown; ++at) {
                const app::PersonalBest& best = bests.at(at);
                parts.push_back(ftxui::text(best.mode + " " + best.metric + " ") | ftxui::color(muted.color));
                parts.push_back(ftxui::text(app::json::number(best.value) + "   ") | ftxui::color(accent.color));
            }
            return ftxui::hbox(std::move(parts));
        }

    }  // namespace

    ftxui::Element HistoryScreen::summary_rows(const Styling& accent, const Styling& muted) const {
        return ftxui::vbox({
                ftxui::hbox({
                        ftxui::text("  " + runs_text(data_.totals.sessions)) | ftxui::color(accent.color),
                        ftxui::text(" · " + duration_text(data_.totals.total_time) + " typed") |
                                ftxui::color(muted.color),
                        ftxui::text(" · mean " + whole(data_.totals.mean_net_wpm.value) + " wpm") |
                                ftxui::color(muted.color),
                        ftxui::text(" · streak " + std::to_string(data_.streak.current) + " (best " +
                                    std::to_string(data_.streak.longest) + ")") |
                                ftxui::color(muted.color),
                }),
                ftxui::hbox({
                        ftxui::text("  today    ") | ftxui::color(muted.color),
                        ftxui::text(runs_text(data_.today.runs) + ", " + duration_text(data_.today.typed)) |
                                ftxui::color(accent.color),
                        // Said either way. "Goal met" alone leaves somebody
                        // wondering whether the line failed to draw or the day
                        // did.
                        ftxui::text(data_.today.met ? "  goal met" : "  goal not met yet") |
                                ftxui::color(data_.today.met ? accent.color : muted.color),
                }),
                bests_line(data_.bests, muted, accent),
        });
    }

    ftxui::Element HistoryScreen::session_rows(const Styling& accent, const Styling& muted) const {
        std::vector<ftxui::Element> rows{ftxui::text("  Recent") | ftxui::color(muted.color)};
        const std::size_t last = std::min(data_.sessions.size(), first_row_ + page_size());
        for (std::size_t at = first_row_; at < last; ++at) {
            const app::SessionRow& row = data_.sessions.at(at);
            const bool here = at == selected_ && focused_ == HistoryField::Sessions;
            std::string line = "  ";
            line += here ? "> " : "  ";
            line += fixed_width(row.mode, 6) + "  ";
            line += fixed_width(whole(row.net_wpm.value), 3) + " wpm  ";
            line += fixed_width(whole(row.accuracy.value * 100.0) + "%", 4);
            line += row.completed ? "" : "  (abandoned)";
            rows.push_back(ftxui::text(line) | ftxui::color(here ? accent.color : muted.color));
        }
        return ftxui::vbox(std::move(rows));
    }

    ftxui::Element HistoryScreen::export_rows(const Styling& accent, const Styling& muted) const {
        std::vector<ftxui::Element> rows{ftxui::text("")};
        if (exporting_) {
            rows.push_back(ftxui::hbox({
                    ftxui::text("  export to ") | ftxui::color(muted.color),
                    ftxui::text(export_path_ + "_") | ftxui::color(accent.color),
                    ftxui::text("   (.json for JSON, anything else CSV)") | ftxui::color(muted.color),
            }));
        }
        if (!export_message_.empty()) {
            rows.push_back(ftxui::text("  " + export_message_) | ftxui::color(muted.color));
        }
        return rows.empty() ? ftxui::text("") : ftxui::vbox(std::move(rows));
    }

    ftxui::Element HistoryScreen::render() {
        const app::ColorDepth depth = context_->capabilities.color;
        const Styling accent = style_for(*context_->theme, app::ThemeColor::Accent, depth);
        const Styling muted = style_for(*context_->theme, app::ThemeColor::Muted, depth);
        const Styling error = style_for(*context_->theme, app::ThemeColor::Error, depth);
        const Layout layout = context_->layout();

        std::vector<ftxui::Element> rows;
        rows.push_back(ftxui::hbox({
                ftxui::text("History  ") | ftxui::color(accent.color),
                ftxui::text("mode ") | ftxui::color(muted.color),
                ftxui::text(filter_.mode.value_or("all") + "  ") |
                        ftxui::color(focused_ == HistoryField::Mode ? accent.color : muted.color),
                ftxui::text("range ") | ftxui::color(muted.color),
                ftxui::text(std::string{to_string(range_)}) |
                        ftxui::color(focused_ == HistoryField::Range ? accent.color : muted.color),
        }));
        rows.push_back(ftxui::text(""));

        if (data_.totals.sessions == 0) {
            // The state a new user opens this on. A sentence beats a table of
            // zeros, which reads as a broken screen rather than an empty one.
            rows.push_back(ftxui::text("  Nothing recorded yet. Finish a run and it will show up here.") |
                           ftxui::color(muted.color));
        } else {
            LineChartData chart;
            for (std::size_t at = 0; at < data_.trend.size(); ++at) {
                chart.series.push_back({.x = static_cast<double>(at), .y = data_.trend.at(at).mean_net_wpm.value});
            }
            chart.empty_message = "not enough days to plot a trend yet";
            rows.push_back(line_chart(chart, *context_->theme,
                                      {.width = layout.text_columns,
                                       .height = roomy() ? kChartRows - 1 : kShortChartRows - 1,
                                       .depth = depth}));
            rows.push_back(ftxui::text(""));
            rows.push_back(summary_rows(accent, muted));
            rows.push_back(ftxui::text(""));

            if (roomy()) {
                rows.push_back(ftxui::text("  Errors by key") | ftxui::color(muted.color));
                rows.push_back(heatmap(data_.keys, *context_->theme,
                                       {.glyphs = context_->capabilities.glyphs, .depth = depth}));
                rows.push_back(ftxui::text(""));
            }
            rows.push_back(session_rows(accent, muted));
        }

        if (exporting_ || !export_message_.empty()) {
            // Only when there is something to say. An empty element here is
            // still a row, and a blank line that appears for no reason is a
            // layout that shifts under the reader.
            rows.push_back(export_rows(accent, muted));
        }

        for (const std::string& problem: data_.problems) {
            // In place of the part that failed, never instead of the screen.
            rows.push_back(ftxui::text("  " + problem) | ftxui::color(error.color));
        }

        rows.push_back(ftxui::text(""));
        rows.push_back(key_hint_bar(
                {{.action = Action::Export, .label = "export"}, {.action = Action::QuitOrBack, .label = "back"}},
                *context_->keymap, *context_->theme, depth));
        return ftxui::vbox(std::move(rows));
    }

    bool HistoryScreen::handle_export_prompt(const ftxui::Event& event) {
        // The prompt owns the keyboard while it is open, so a path containing a
        // `j` does not cycle a filter behind it. Every event is swallowed for
        // the same reason.
        if (event == ftxui::Event::Return) {
            write_export();
        } else if (event == ftxui::Event::Escape) {
            exporting_ = false;
            export_message_.clear();
        } else if (event == ftxui::Event::Backspace) {
            if (!export_path_.empty()) {
                export_path_.pop_back();
            }
        } else if (event.is_character() && event.character().size() == 1) {
            export_path_ += event.character().front();
        }
        return true;
    }

    bool HistoryScreen::handle_session_list(const ftxui::Event& event) {
        if (event == ftxui::Event::Return && !data_.sessions.empty()) {
            opened_ = data_.sessions.at(selected_).id;
            return true;
        }
        if (event == ftxui::Event::ArrowDown) {
            move_selection(1);
            return true;
        }
        if (event == ftxui::Event::ArrowUp) {
            move_selection(-1);
            return true;
        }
        if (event == ftxui::Event::PageDown) {
            move_selection(static_cast<std::int64_t>(page_size()));
            return true;
        }
        if (event == ftxui::Event::PageUp) {
            move_selection(-static_cast<std::int64_t>(page_size()));
            return true;
        }
        return false;
    }

    bool HistoryScreen::on_event(ftxui::Event event) {
        if (exporting_) {
            return handle_export_prompt(event);
        }
        if (context_->keymap->action_for(event) == Action::Export) {
            exporting_ = true;
            export_message_.clear();
            return true;
        }
        if (event == ftxui::Event::Tab) {
            focused_ = focused_ == HistoryField::Sessions
                               ? HistoryField::Mode
                               : static_cast<HistoryField>(static_cast<std::uint8_t>(focused_) + 1);
            return true;
        }
        if (focused_ == HistoryField::Sessions) {
            return handle_session_list(event);
        }

        if (event != ftxui::Event::ArrowLeft && event != ftxui::Event::ArrowRight) {
            return false;
        }
        const bool forward = event == ftxui::Event::ArrowRight;
        if (focused_ == HistoryField::Mode) {
            cycle_mode(forward);
        } else {
            cycle_range(forward);
        }
        return true;
    }

}  // namespace typeit::tui
