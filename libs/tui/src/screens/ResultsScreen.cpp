#include "screens/ResultsScreen.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <ftxui/component/event.hpp>
#include <ftxui/dom/elements.hpp>
#include <map>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "Bars.h"
#include "Charts.h"
#include "ColorQuantizer.h"
#include "Keymap.h"
#include "typeit/app/Json.h"

namespace typeit::tui {
    namespace {

        /// One number and its name, in a fixed-width field so the block does
        /// not shift between a run of 9 WPM and one of 100.
        ftxui::Element figure(const std::string& label, const std::string& value, const Styling& name,
                              const Styling& number) {
            return ftxui::hbox({
                    ftxui::text("  " + label + std::string(label.size() < 18 ? 18 - label.size() : 1, ' ')) |
                            ftxui::color(name.color),
                    ftxui::text(fixed_width(value, 8)) | ftxui::color(number.color),
            });
        }

        /// Two decimals, which is what a person reads.
        ///
        /// The export keeps the full precision; a results screen is not an
        /// interchange format. `json::number` gives six decimals, so a net WPM
        /// of 27.972028 is nine characters in an eight-wide field — and the
        /// field truncates from the *left*, so a 27 WPM run was displayed as
        /// 7.972028. Rounding here rather than widening the field, because a
        /// column that grows with the precision of the number in it stops being
        /// a column.
        std::string rounded(double value) { return app::json::number(std::round(value * 100.0) / 100.0); }

        std::string percent(core::Accuracy value) { return rounded(value.value * 100.0) + "%"; }

        /// A difference, signed, so "+4.2" and "-4.2" read at a glance.
        std::string signed_delta(double value) { return (value >= 0.0 ? "+" : "") + rounded(value); }

        /// How many entries each list gets. Five, which is UX §3.4's figure and
        /// about as many as anybody acts on.
        constexpr std::size_t kListLength = 5;

        /// The `count` worst of a map, biggest first. Ties break on the key so
        /// two runs with the same mistakes list them in the same order — a
        /// results screen that reshuffled between draws would be unreadable.
        template<typename Key>
        std::vector<std::pair<Key, std::size_t>> worst(const std::map<Key, std::size_t>& counts, std::size_t count) {
            std::vector<std::pair<Key, std::size_t>> sorted{counts.begin(), counts.end()};
            std::ranges::sort(sorted, [](const auto& left, const auto& right) {
                return left.second != right.second ? left.second > right.second : left.first < right.first;
            });
            if (sorted.size() > count) {
                sorted.resize(count);
            }
            return sorted;
        }

        /// The slowest bigrams by mean latency, over those actually measured.
        /// A bigram with no latency samples is not slow, it is unmeasured.
        std::vector<std::pair<std::string, core::Millis>> slowest(const core::KeyStats& stats, std::size_t count) {
            std::vector<std::pair<std::string, core::Millis>> measured;
            for (const auto& [bigram, stat]: stats.per_bigram) {
                if (stat.latency_samples > 0) {
                    measured.emplace_back(bigram, stat.mean_latency());
                }
            }
            std::ranges::sort(measured, [](const auto& left, const auto& right) {
                return left.second != right.second ? left.second > right.second : left.first < right.first;
            });
            if (measured.size() > count) {
                measured.resize(count);
            }
            return measured;
        }

    }  // namespace

    void ResultsScreen::load_comparison() {
        // Read here, once. The history does not change while somebody looks at
        // their own result.
        const HistorySource& source = context_->history;
        if (source.records == nullptr) {
            return;
        }

        app::HistoryFilter filter;
        filter.mode = result_.record.mode;
        if (const core::Result<app::Aggregates> totals = source.records->aggregates(filter); totals) {
            // One session is this one. With nothing else to compare against
            // there is no comparison — a line reading "+0 vs average" on a
            // first run invents a baseline out of the run itself.
            if (totals->sessions > 1) {
                comparison_.baseline =
                        Baseline{.mean_net_wpm = totals->mean_net_wpm, .best_net_wpm = totals->best_net_wpm};
                comparison_.personal_best =
                        result_.record.completed && result_.record.net_wpm.value >= totals->best_net_wpm.value;
            } else if (totals->sessions == 1 && result_.record.completed) {
                // The very first completed run is a personal best by
                // definition, and saying so is the point.
                comparison_.personal_best = true;
            }
        }
    }

    ftxui::Element ResultsScreen::render() {
        const app::ColorDepth depth = context_->capabilities.color;
        const Styling accent = style_for(*context_->theme, app::ThemeColor::Accent, depth);
        const Styling muted = style_for(*context_->theme, app::ThemeColor::Muted, depth);
        const Styling plain = style_for(*context_->theme, app::ThemeColor::TextCorrect, depth);
        // Multi-byte graphemes go through unchanged — the pair list is the one
        // place a `ć` is most likely to appear, since it is the one people miss.
        const std::string arrow = context_->capabilities.glyphs == app::GlyphSet::Ascii ? "->" : "→";

        std::vector<ftxui::Element> rows;
        rows.push_back(ftxui::text(result_.record.completed ? "Run complete" : "Run abandoned") |
                       ftxui::color(result_.record.completed ? accent.color : muted.color));
        rows.push_back(ftxui::text(""));

        // Every number goes through the same helper the exports use, rounded
        // for reading: a figure on screen and the same figure in a CSV cannot
        // disagree about anything but the digits nobody reads.
        rows.push_back(figure("net wpm", rounded(result_.record.net_wpm.value), muted, plain));
        rows.push_back(figure("gross wpm", rounded(result_.record.gross_wpm.value), muted, plain));
        rows.push_back(figure("raw wpm", rounded(result_.record.raw_wpm.value), muted, plain));
        rows.push_back(figure("accuracy", percent(result_.record.accuracy), muted, plain));
        rows.push_back(figure("correctness", percent(result_.record.final_correctness), muted, plain));
        rows.push_back(figure("consistency", rounded(result_.record.consistency), muted, plain));
        rows.push_back(figure("characters", std::to_string(result_.record.graphemes_typed), muted, plain));
        rows.push_back(figure("errors", std::to_string(result_.record.errors_total), muted, plain));
        rows.push_back(
                figure("seconds", rounded(static_cast<double>(result_.record.duration.value) / 1000.0), muted, plain));

        if (comparison_.personal_best) {
            rows.push_back(ftxui::text(""));
            rows.push_back(ftxui::text("  a personal best") | ftxui::color(accent.color));
        }
        if (comparison_.baseline.has_value()) {
            const Baseline& against = *comparison_.baseline;
            rows.push_back(ftxui::text(""));
            rows.push_back(ftxui::text("  " + signed_delta(result_.record.net_wpm.value - against.mean_net_wpm.value) +
                                       " wpm vs your average, " +
                                       signed_delta(result_.record.net_wpm.value - against.best_net_wpm.value) +
                                       " vs your best") |
                           ftxui::color(muted.color));
        }

        if (!result_.record.timeline.empty()) {
            rows.push_back(ftxui::text(""));
            LineChartData chart;
            for (const core::TimelineSample& sample: result_.record.timeline) {
                chart.series.push_back({.x = static_cast<double>(sample.at.value), .y = sample.wpm.value});
                for (std::size_t at = 0; at < sample.errors; ++at) {
                    chart.errors.push_back(static_cast<double>(sample.at.value));
                }
            }
            rows.push_back(line_chart(chart, *context_->theme,
                                      {.width = context_->layout().text_columns, .height = 6, .depth = depth}));
        }

        const std::vector<std::pair<std::pair<std::string, std::string>, std::size_t>> pairs =
                worst(result_.errors.substitutions, kListLength);
        if (!pairs.empty()) {
            rows.push_back(ftxui::text(""));
            std::string line = "  missed  ";
            for (const auto& [pair, count]: pairs) {
                // `expected → typed`, which is the only way round that says
                // anything: "I typed n for m" is the mistake.
                line += pair.first + arrow + pair.second + " " + std::to_string(count) + "   ";
            }
            rows.push_back(ftxui::text(line) | ftxui::color(muted.color));
        }

        const std::vector<std::pair<std::string, core::Millis>> slow = slowest(result_.keys, kListLength);
        if (!slow.empty()) {
            std::string line = "  slowest ";
            for (const auto& [bigram, latency]: slow) {
                line += bigram + " " + std::to_string(latency.value) + "ms   ";
            }
            rows.push_back(ftxui::text(line) | ftxui::color(muted.color));
        }

        rows.push_back(ftxui::text(""));
        const std::vector<Hint> hints{
                {.action = Action::Restart, .label = "again"},
                {.action = Action::NewText, .label = "new text"},
                {.action = Action::Menu, .label = "menu"},
        };
        rows.push_back(key_hint_bar(hints, *context_->keymap, *context_->theme, depth));

        return ftxui::vbox(std::move(rows));
    }

    bool ResultsScreen::on_event(ftxui::Event event) {
        const std::optional<Action> action = context_->keymap->action_for(event);
        if (!action.has_value()) {
            return false;
        }

        switch (*action) {
            case Action::Restart:
                requested_ = ResultsAction::Restart;
                return true;
            case Action::NewText:
                requested_ = ResultsAction::NewText;
                return true;
            case Action::QuitOrBack:
            case Action::Menu:
                requested_ = ResultsAction::Menu;
                return true;
            default:
                // Everything else — quit, history, the library — belongs to
                // whoever is driving the stack.
                return false;
        }
    }

    std::optional<ResultsAction> ResultsScreen::take_action() {
        std::optional<ResultsAction> taken = requested_;
        requested_.reset();
        return taken;
    }

}  // namespace typeit::tui
