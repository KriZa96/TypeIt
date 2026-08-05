#include "typeit/app/services/SessionService.h"

#include <algorithm>
#include <cstddef>
#include <memory>
#include <string>
#include <utility>

#include "typeit/app/records/History.h"
#include "typeit/core/Version.h"
#include "typeit/core/metrics/ErrorMap.h"
#include "typeit/core/metrics/KeyStats.h"
#include "typeit/core/metrics/Metrics.h"
#include "typeit/core/metrics/Timeline.h"
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

    }  // namespace

    core::Result<ActiveRun> SessionService::start(const SessionRequest& request) const {
        core::Result<std::unique_ptr<core::IMode>> mode = modes_->create(request.mode);
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

        const core::Result<core::SessionId> id =
                history_->save_run(record, core::key_stats(log, target), core::error_map(log, target));
        if (!id) {
            return std::unexpected{id.error()};
        }
        return SessionResult{.id = *id, .record = std::move(record)};
    }

}  // namespace typeit::app
