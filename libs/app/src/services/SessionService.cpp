#include "typeit/app/services/SessionService.h"

#include <algorithm>
#include <cstddef>
#include <memory>
#include <span>
#include <string>
#include <utility>

#include "typeit/app/Json.h"
#include "typeit/app/records/History.h"
#include "typeit/core/Version.h"
#include "typeit/core/metrics/ErrorMap.h"
#include "typeit/core/metrics/KeyStats.h"
#include "typeit/core/metrics/Metrics.h"
#include "typeit/core/metrics/Timeline.h"
#include "typeit/core/modes/RaceMode.h"
#include "typeit/core/session/Keystroke.h"
#include "typeit/core/session/KeystrokeLog.h"
#include "typeit/core/session/Session.h"
#include "typeit/core/session/TypingModel.h"
#include "typeit/core/text/TextBuffer.h"
#include "typeit/core/text_supply/ChunkedProvider.h"
#include "typeit/core/text_supply/ITextProvider.h"
#include "typeit/core/text_supply/WholeTextProvider.h"
#include "typeit/core/util/Result.h"
#include "typeit/core/util/Units.h"

namespace typeit::app {
    namespace {

        /// The whole text, or the next chunk of it. Two providers is the whole
        /// choice today; a third would be a reason to move this behind an
        /// interface, and two is not.
        [[nodiscard]] core::Result<std::unique_ptr<core::ITextProvider>> make_provider(const SessionRequest& request) {
            if (!request.resume_at.has_value()) {
                return std::make_unique<core::WholeTextProvider>(request.text, request.seed);
            }

            core::ChunkedProvider::Options options;
            options.offset = *request.resume_at;
            options.seed = request.seed;
            if (request.chunk_graphemes != 0) {
                options.chunk_graphemes = request.chunk_graphemes;
            }

            core::Result<std::unique_ptr<core::ChunkedProvider>> chunked =
                    core::ChunkedProvider::create(request.text, options);
            if (!chunked) {
                return std::unexpected{chunked.error()};
            }
            return std::unique_ptr<core::ITextProvider>{std::move(*chunked)};
        }

        [[nodiscard]] std::size_t count_of(const core::TypingModel& model, core::GraphemeState state) {
            return static_cast<std::size_t>(std::ranges::count(model.states(), state));
        }


        /// The ghost's curve, laid onto the timeline the metrics produced.
        ///
        /// Both are one-second series but neither is guaranteed to be complete:
        /// a timeline bucket exists only where something was typed, and a pacer
        /// sample only after the opening grace. So each bucket takes the last
        /// pacer reading at or before it, which is what the ghost's speed
        /// actually was during that second.
        void merge_pacer_curve(std::vector<core::TimelineSample>& timeline, std::span<const core::PacerSample> curve) {
            if (curve.empty()) {
                return;
            }
            auto reading = curve.begin();
            for (core::TimelineSample& sample: timeline) {
                while (std::next(reading) != curve.end() && std::next(reading)->at <= sample.at) {
                    ++reading;
                }
                if (reading->at <= sample.at) {
                    sample.pacer_wpm = reading->wpm;
                }
            }
        }

        /// Every effective parameter, so a past race is reconstructible after
        /// the config changes.
        ///
        /// Written out rather than looped, because the field names are the
        /// contract: a reader five years from now is matching these against
        /// GAMEPLAY §3.5, not against whatever a reflection helper produced.
        [[nodiscard]] std::string race_params_json(const core::RaceParams& params) {
            std::string out = R"({"kind":"race")";
            const auto field = [&out](std::string_view name, double value) {
                out += ",";
                json::append_string(out, name);
                out += ":" + json::number(value);
            };
            field("ramp_up", params.ramp_up);
            field("ramp_down", params.ramp_down);
            field("min_accuracy", params.min_accuracy.value);
            field("lead_comfort", params.lead_comfort);
            field("lead_danger", params.lead_danger);
            field("lead_scale", params.lead_scale);
            field("grace_ms", static_cast<double>(params.grace.value));
            field("lives", static_cast<double>(params.lives));
            field("catch_penalty", params.catch_penalty);
            field("start_factor", params.start_factor);
            field("sustain_window_ms", static_cast<double>(params.sustain_window.value));
            field("min_speed", params.min_speed.value);
            field("max_speed", params.max_speed.value);
            out += "}";
            return out;
        }

    }  // namespace

    core::Result<ActiveRun> SessionService::start(const SessionRequest& request) const {
        core::Result<std::unique_ptr<core::IMode>> mode = modes_->create(request.mode, request.params);
        if (!mode) {
            return std::unexpected{mode.error()};
        }

        core::Result<std::unique_ptr<core::ITextProvider>> provider = make_provider(request);
        if (!provider) {
            return std::unexpected{provider.error()};
        }

        core::Result<std::unique_ptr<core::Session>> session = core::Session::create(
                core::Session::Parts{
                        .mode = std::move(*mode), .provider = std::move(*provider), .rules = request.rules},
                clock_->now());
        if (!session) {
            return std::unexpected{session.error()};
        }

        // Checked on the built session rather than on the request, so that a
        // bookmark parked at the end of a text — a chunk that is empty although
        // the text is not — is caught by the same guard.
        if ((*session)->text().empty()) {
            return core::fail(core::ErrorCode::EmptyText, request.mode);
        }

        return ActiveRun{
                .session = std::move(*session),
                .started_at = wall_clock_->unix_now(),
                .mode_param = request.mode_param,
                .text_id = request.text_id,
                .provider = request.resume_at.has_value() ? "chunked" : "whole",
        };
    }

    core::Result<SessionResult> SessionService::finish(const ActiveRun& run, Outcome outcome) const {
        const core::Session& session = *run.session;
        const core::KeystrokeLog& log = session.model().log();
        const core::TextBuffer& target = session.text();

        const core::SpeedMetrics speeds = core::speed(log, target);
        const core::AccuracyMetrics correctness = core::accuracy(log, target);

        std::size_t typed = 0;
        std::size_t backspaces = 0;
        for (const core::Keystroke& event: log.events()) {
            if (event.kind == core::KeystrokeKind::Backspace) {
                ++backspaces;
            } else {
                ++typed;
            }
        }

        // A race records three things no other mode has: the highest speed it
        // held, the speed accuracy collapsed at, and the ghost's whole curve.
        // Asked for by type rather than through `IMode`, because a mode that
        // could be asked for a pacer speed would be every mode carrying a
        // question only one of them can answer (TI-127).
        const auto* const race = dynamic_cast<const core::RaceMode*>(&session.mode());

        SessionRecord record;
        record.started_at = run.started_at;
        record.ended_at = wall_clock_->unix_now();
        record.mode = session.mode().id();
        record.mode_param = run.mode_param;
        record.text_id = run.text_id;
        record.provider = run.provider;
        record.provider_seed = session.provider().seed();

        // The log's own span, first keystroke to last — the same denominator
        // the speeds above were divided by. Taking it from the wall clock
        // instead would put a duration in the history that the WPM beside it
        // does not follow from, which is defect C5 wearing a different hat.
        record.duration = log.duration();

        record.graphemes_typed = typed;
        // Correct and Corrected both stand as correct at the end; the
        // difference between them is a question about accuracy, which
        // `first_attempt_errors` below answers separately.
        record.graphemes_correct = count_of(session.model(), core::GraphemeState::Correct) +
                                   count_of(session.model(), core::GraphemeState::Corrected);
        record.errors_total = correctness.first_attempt_errors;
        record.errors_uncorrected = count_of(session.model(), core::GraphemeState::Incorrect);
        record.backspaces = backspaces;

        record.raw_wpm = speeds.raw;
        record.gross_wpm = speeds.gross;
        record.net_wpm = speeds.net;
        record.accuracy = correctness.accuracy;
        record.final_correctness = correctness.final_correctness;
        record.consistency = core::consistency(log, target);

        record.completed = outcome == Outcome::Completed;
        record.app_version = kVersionString;
        record.timeline = core::timeline(log, target);
        if (race != nullptr) {
            record.peak_wpm = race->race().peak_sustained;
            // Absent rather than zero when accuracy never collapsed: there was
            // no wall, and a wall at 0 WPM is a chart with a mark on it saying
            // nothing happened (TI-128 reads this).
            if (race->race().wall.value > 0.0) {
                record.wall_wpm = race->race().wall;
            }
            merge_pacer_curve(record.timeline, race->pacer_curve());
            if (record.mode_param.empty()) {
                // The numbers this race was *run* by, not the ones in the
                // config file when somebody later looks at it. A preset edited
                // next week must not rewrite what last week's race was.
                record.mode_param = race_params_json(race->params());
            }
        }

        core::KeyStats keys = core::key_stats(log, target);
        core::ErrorMap errors = core::error_map(log, target);

        const core::Result<core::SessionId> id = history_->save_run(record, keys, errors);
        if (!id) {
            return std::unexpected{id.error()};
        }
        return SessionResult{
                .id = *id, .record = std::move(record), .keys = std::move(keys), .errors = std::move(errors)};
    }

}  // namespace typeit::app
