#include "screens/SessionScreen.h"

#include <cstdint>
#include <ftxui/component/event.hpp>
#include <ftxui/dom/elements.hpp>
#include <memory>
#include <optional>
#include <utility>
#include <variant>
#include <vector>

#include "Bars.h"
#include "ColorQuantizer.h"
#include "Keymap.h"
#include "typeit/core/metrics/Metrics.h"
#include "typeit/core/modes/IMode.h"

namespace typeit::tui {
    namespace {

        constexpr std::int64_t kMillisPerSecond = 1'000;

        /// What the bar shows, read from the run rather than accumulated
        /// alongside it — the model is the only source of truth for a number.
        SessionStats stats_of(const core::Session& session) {
            const core::SpeedMetrics speeds = core::speed(session.model().log(), session.text());
            const core::AccuracyMetrics correctness = core::accuracy(session.model().log(), session.text());

            SessionStats stats;
            stats.wpm = speeds.net;
            stats.accuracy = correctness.accuracy;

            // Whatever the mode has to say about progress. A variant, so the
            // bar cannot read a field this mode never fills in.
            const core::ModeProgress progress = session.progress();
            if (const auto* const timed = std::get_if<core::TimedProgress>(&progress); timed != nullptr) {
                stats.seconds = timed->remaining.value / kMillisPerSecond;
            } else if (const auto* const words = std::get_if<core::WordProgress>(&progress); words != nullptr) {
                stats.seconds = -1;
                stats.words_done = words->done;
                stats.words_total = words->total;
            } else {
                stats.seconds = -1;
            }
            return stats;
        }

    }  // namespace

    core::Result<std::unique_ptr<SessionScreen>> SessionScreen::create(const ScreenContext& context,
                                                                       const app::SessionService& service,
                                                                       const app::SessionRequest& request,
                                                                       const core::IClock& clock) {
        core::Result<app::ActiveRun> run = service.start(request);
        if (!run) {
            return std::unexpected{run.error()};
        }
        // Not make_unique: the constructor is private, because a screen with a
        // half-started run is not a thing to hand anybody.
        return std::unique_ptr<SessionScreen>{new SessionScreen{context, service, std::move(*run), clock}};
    }

    SessionScreen::SessionScreen(const ScreenContext& context, const app::SessionService& service, app::ActiveRun run,
                                 const core::IClock& clock) :
        context_{&context}, service_{&service}, clock_{&clock}, run_{std::move(run)},
        countdown_left_{context.config->general.countdown_s}, started_ticking_{clock.now()} {
        TypingAreaOptions options;
        const Layout layout = context.layout();
        options.columns = layout.text_columns;
        options.lines_visible = layout.lines_visible;
        options.depth = context.capabilities.color;
        options.glyphs = context.capabilities.glyphs;
        options.blind = context.config->appearance.caret_blink ? false : context.config->typing.blind_mode;

        area_ = std::make_shared<TypingArea>(*run_.session, *context.theme, clock, options);
    }

    void SessionScreen::on_tick(core::Millis now) {
        if (countdown_left_ > 0) {
            const std::int64_t elapsed = (now - started_ticking_).value / kMillisPerSecond;
            countdown_left_ = std::max<std::int64_t>(0, context_->config->general.countdown_s - elapsed);
            // Time is not offered to the mode during the countdown: a run that
            // had already started counting before the typist could type would
            // charge them for the wait.
            return;
        }

        run_.session->on_tick(now);
        if (run_.session->is_finished() && !result_.has_value()) {
            // Exactly once. A mode that reported finished on every subsequent
            // tick would otherwise save the run again and again.
            finish(app::Outcome::Completed, SessionOutcome::Finished);
        }
    }

    void SessionScreen::finish(app::Outcome outcome, SessionOutcome reported) {
        if (result_.has_value()) {
            return;
        }
        if (const core::Result<app::SessionResult> saved = service_->finish(run_, outcome); saved) {
            result_ = *saved;
        }
        outcome_ = reported;
    }

    void SessionScreen::abandon() { finish(app::Outcome::Abandoned, SessionOutcome::Abandoned); }

    ftxui::Element SessionScreen::render() {
        const app::ColorDepth depth = context_->capabilities.color;
        const Layout layout = context_->layout();

        std::vector<ftxui::Element> rows;
        if (layout.show_stats_bar) {
            StatsBarOptions bar;
            bar.show_wpm = context_->config->appearance.show_live_wpm;
            bar.show_accuracy = context_->config->appearance.show_live_acc;
            bar.show_progress = context_->config->appearance.show_progress;
            bar.depth = depth;
            rows.push_back(stats_bar(stats_of(*run_.session), *context_->theme, bar));
            rows.push_back(ftxui::text(""));
        }

        if (countdown_left_ > 0) {
            const Styling accent = style_for(*context_->theme, app::ThemeColor::Accent, depth);
            rows.push_back(ftxui::text("  " + std::to_string(countdown_left_)) | ftxui::color(accent.color));
        } else {
            rows.push_back(area_->Render());
        }

        if (layout.show_hint_bar) {
            rows.push_back(ftxui::text(""));
            rows.push_back(key_hint_bar(
                    {{.action = Action::Restart, .label = "restart"}, {.action = Action::QuitOrBack, .label = "menu"}},
                    *context_->keymap, *context_->theme, depth));
        }
        return ftxui::vbox(std::move(rows));
    }

    bool SessionScreen::on_event(ftxui::Event event) {
        if (result_.has_value()) {
            // Over. Keys belong to whatever comes next.
            return false;
        }

        if (const std::optional<Action> action = context_->keymap->action_for(event); action.has_value()) {
            if (*action == Action::Restart) {
                // Saved as abandoned first: the run happened, and a restart is
                // not a reason to pretend it did not.
                finish(app::Outcome::Abandoned, SessionOutcome::Restart);
                return true;
            }
            if (*action == Action::QuitOrBack || *action == Action::Menu) {
                abandon();
                return true;
            }
        }

        if (countdown_left_ > 0) {
            // Nothing to type into yet. The keystroke is dropped rather than
            // queued: a character typed during the countdown was aimed at a
            // screen that was not ready for it.
            return true;
        }

        // The one place the model mutates.
        return area_->OnEvent(std::move(event));
    }

    std::optional<SessionOutcome> SessionScreen::take_outcome() {
        std::optional<SessionOutcome> taken = outcome_;
        outcome_.reset();
        return taken;
    }

}  // namespace typeit::tui
