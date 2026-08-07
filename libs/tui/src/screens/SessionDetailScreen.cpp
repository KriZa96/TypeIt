#include "screens/SessionDetailScreen.h"

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <ftxui/component/event.hpp>
#include <ftxui/dom/elements.hpp>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "Bars.h"
#include "Charts.h"
#include "ColorQuantizer.h"
#include "Keymap.h"
#include "typeit/app/Json.h"
#include "typeit/core/Version.h"

namespace typeit::tui {
    namespace {

        constexpr std::size_t kLabelWidth = 18;
        constexpr std::size_t kValueWidth = 10;
        constexpr double kMillisPerSecond = 1'000.0;

        /// Two decimals, for the same reason the results screen rounds: a
        /// results view is not an interchange format, and six decimals in a
        /// ten-wide field is a number with its first digit missing.
        std::string rounded(double value) { return app::json::number(std::round(value * 100.0) / 100.0); }

        ftxui::Element figure(const std::string& label, const std::string& value, const Styling& name,
                              const Styling& number) {
            return ftxui::hbox({
                    ftxui::text("  " + label +
                                std::string(label.size() < kLabelWidth ? kLabelWidth - label.size() : 1, ' ')) |
                            ftxui::color(name.color),
                    ftxui::text(fixed_width(value, kValueWidth)) | ftxui::color(number.color),
            });
        }

    }  // namespace

    bool recorded_by_this_major(std::string_view app_version) {
        // Everything up to the first dot. An empty or unparseable version is
        // *not* this major — a row written by something that did not say what
        // wrote it is exactly the row worth flagging.
        const std::size_t dot = app_version.find('.');
        if (dot == std::string_view::npos) {
            return false;
        }
        return app_version.substr(0, dot) == std::to_string(kVersionMajor);
    }

    SessionDetailScreen::SessionDetailScreen(const ScreenContext& context, core::SessionId id) : context_{&context} {
        if (context_->history.records == nullptr) {
            problem_ = "no history is available";
            return;
        }
        if (core::Result<app::SessionRecord> found = context_->history.records->session(id); found) {
            record_ = std::move(*found);
        } else {
            // Named rather than blank: a stale id and a broken database look
            // identical from a page of zeros.
            problem_ = core::to_string(found.error());
        }
    }

    bool SessionDetailScreen::take_back() {
        const bool asked = back_;
        back_ = false;
        return asked;
    }

    ftxui::Element SessionDetailScreen::render() {
        const app::ColorDepth depth = context_->capabilities.color;
        const Styling accent = style_for(*context_->theme, app::ThemeColor::Accent, depth);
        const Styling muted = style_for(*context_->theme, app::ThemeColor::Muted, depth);
        const Styling warning = style_for(*context_->theme, app::ThemeColor::Warning, depth);
        const Styling error = style_for(*context_->theme, app::ThemeColor::Error, depth);
        const Layout layout = context_->layout();

        std::vector<ftxui::Element> rows;
        if (!record_.has_value()) {
            rows.push_back(ftxui::text("Session") | ftxui::color(accent.color));
            rows.push_back(ftxui::text(""));
            rows.push_back(ftxui::text("  " + problem_) | ftxui::color(error.color));
            rows.push_back(ftxui::text(""));
            rows.push_back(key_hint_bar({{.action = Action::QuitOrBack, .label = "back"}}, *context_->keymap,
                                        *context_->theme, depth));
            return ftxui::vbox(std::move(rows));
        }

        const app::SessionRecord& run = *record_;
        rows.push_back(ftxui::hbox({
                ftxui::text(run.completed ? "Session  " : "Session (abandoned)  ") |
                        ftxui::color(run.completed ? accent.color : muted.color),
                ftxui::text(run.mode + "  " + run.mode_param) | ftxui::color(muted.color),
        }));

        if (!recorded_by_this_major(run.app_version)) {
            // What WPM and accuracy *mean* changed at 2.0 (VERSIONING §9), so
            // an older run's figures are not comparable with today's. Showing
            // them beside each other without saying so is the quiet kind of
            // wrong.
            rows.push_back(ftxui::text("  recorded by " +
                                       (run.app_version.empty() ? std::string{"an unknown version"} : run.app_version) +
                                       "; metrics may not be comparable") |
                           ftxui::color(warning.color));
        }
        rows.push_back(ftxui::text(""));

        rows.push_back(figure("net wpm", rounded(run.net_wpm.value), muted, accent));
        rows.push_back(figure("gross wpm", rounded(run.gross_wpm.value), muted, accent));
        rows.push_back(figure("raw wpm", rounded(run.raw_wpm.value), muted, accent));
        rows.push_back(figure("accuracy", rounded(run.accuracy.value * 100.0) + "%", muted, accent));
        rows.push_back(figure("correctness", rounded(run.final_correctness.value * 100.0) + "%", muted, accent));
        rows.push_back(figure("consistency", rounded(run.consistency), muted, accent));
        rows.push_back(figure("characters", std::to_string(run.graphemes_typed), muted, accent));
        rows.push_back(figure("correct", std::to_string(run.graphemes_correct), muted, accent));
        rows.push_back(figure("errors", std::to_string(run.errors_total), muted, accent));
        rows.push_back(figure("uncorrected", std::to_string(run.errors_uncorrected), muted, accent));
        rows.push_back(figure("backspaces", std::to_string(run.backspaces), muted, accent));
        rows.push_back(
                figure("seconds", rounded(static_cast<double>(run.duration.value) / kMillisPerSecond), muted, accent));

        rows.push_back(ftxui::text(""));
        LineChartData chart;
        for (const core::TimelineSample& sample: run.timeline) {
            chart.series.push_back({.x = static_cast<double>(sample.at.value), .y = sample.wpm.value});
            for (std::size_t at = 0; at < sample.errors; ++at) {
                chart.errors.push_back(static_cast<double>(sample.at.value));
            }
        }
        chart.empty_message = "no per-second samples were recorded for this run";
        rows.push_back(
                line_chart(chart, *context_->theme, {.width = layout.text_columns, .height = 8, .depth = depth}));

        rows.push_back(ftxui::text(""));
        rows.push_back(key_hint_bar({{.action = Action::QuitOrBack, .label = "back"}}, *context_->keymap,
                                    *context_->theme, depth));
        return ftxui::vbox(std::move(rows));
    }

    bool SessionDetailScreen::on_event(ftxui::Event event) {
        const std::optional<Action> action = context_->keymap->action_for(event);
        if (action == Action::QuitOrBack || action == Action::Menu) {
            back_ = true;
            return true;
        }
        return false;
    }

}  // namespace typeit::tui
