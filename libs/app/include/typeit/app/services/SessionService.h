// The central use case: one typing run, from "start" to "saved" (TI-068,
// ARCHITECTURE §5.2).
//
// Two responsibilities, and deliberately no third. Starting resolves a mode,
// a provider and a set of rules into a `core::Session`. Finishing computes
// every metric from the log and writes the run — the session, its samples, the
// records it set, and the key, bigram and error-pair totals — in one
// transaction.
//
// What it does not do is play. The service never sees a keystroke: the screen
// (or `--simulate`) drives the session it was handed, and comes back here only
// when the run is over. That is what makes a headless run the same code path
// as a real one rather than a parallel implementation of it.
//
// The service holds no per-run state. Everything a finished run needs to be
// filed under travels in the `ActiveRun` the caller is holding, so two sessions in
// one process cannot influence each other — the regression guard for the five
// global booleans 1.0 kept its session state in.
#ifndef TYPEIT_APP_SERVICES_SESSIONSERVICE_H
#define TYPEIT_APP_SERVICES_SESSIONSERVICE_H

#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>

#include "typeit/app/ports/IHistoryRepository.h"
#include "typeit/app/records/History.h"
#include "typeit/core/modes/ModeRegistry.h"
#include "typeit/core/session/Session.h"
#include "typeit/core/session/TypingRules.h"
#include "typeit/core/util/IClock.h"
#include "typeit/core/util/Result.h"
#include "typeit/core/util/Units.h"

namespace typeit::app {

    /// What to play. Everything the caller decided, in one value.
    struct SessionRequest {
        /// The id the mode is registered under. A mode's *parameter* — thirty
        /// seconds, fifty words — is baked into the factory that registered it
        /// (ADR-008), so it does not appear here.
        std::string mode;
        /// The same parameter as JSON, recorded verbatim. `IMode` has no way to
        /// describe itself, and a column per mode would mean a migration per
        /// mode, so the caller that chose the parameter is the one that names
        /// it.
        std::string mode_param;

        /// UTF-8, already normalised — normalisation happens at import
        /// (GAMEPLAY §5.2), not at the start of every run.
        std::string text;
        /// Absent for generated text with nothing in the library behind it.
        std::optional<core::TextId> text_id;

        /// Recorded with the run so the exact text stream can be produced
        /// again, which turns a bug report into a deterministic repro.
        std::uint64_t seed = 0;

        /// Where to resume a text being typed in chunks. Absent means the whole
        /// text at once, which is what every mode over a fixed text wants.
        std::optional<core::GraphemeIndex> resume_at;
        /// Zero means the provider's own default.
        std::size_t chunk_graphemes = 0;

        core::TypingRules rules;
    };

    /// A run in progress: the session to drive, and the few facts about it that
    /// only the caller of `start` knows and `finish` cannot recompute.
    ///
    /// Move-only, because the session inside it is immovable and is held
    /// through the one handle that is not.
    struct ActiveRun {
        std::unique_ptr<core::Session> session;

        /// Unix milliseconds — the date this run is filed under. Unlike the
        /// log's own clock, which starts at an arbitrary point, a history that
        /// spans machines needs a real one.
        core::Millis started_at{0};

        std::string mode_param;
        std::optional<core::TextId> text_id;
        /// `"whole"` or `"chunked"`. A name rather than something read off the
        /// provider, which has no way to say what kind it is.
        std::string provider;
    };

    /// How the run ended. An enum rather than a bool because both values are
    /// ordinary outcomes and a bare `true` at a call site says nothing about
    /// which is which.
    enum class Outcome : std::uint8_t {
        Completed,
        /// The user quit. Saved anyway — it happened — and never a record
        /// (GAMEPLAY §7.1).
        Abandoned,
    };

    /// What was saved, so the results screen does not have to read it back.
    struct SessionResult {
        core::SessionId id{0};
        SessionRecord record;
    };

    class SessionService {
    public:
        /// Everything is borrowed and must outlive the service; the composition
        /// root owns all four.
        ///
        /// Two clocks, and they are not interchangeable. The monotonic one
        /// stamps the instant the run was offered, which is what the mode
        /// measures elapsed time against; the wall clock supplies the date the
        /// run is filed under. Measuring a duration against a wall clock gives
        /// a typist 40 WPM on the night the clocks go back.
        SessionService(IHistoryRepository& history, const core::ModeRegistry& modes, const core::IClock& clock,
                       const core::IWallClock& wall_clock) :
            history_{&history}, modes_{&modes}, clock_{&clock}, wall_clock_{&wall_clock} {}

        /// Resolves the mode and the provider and builds the session.
        ///
        /// Nothing is written here — a run that never happened is not history.
        /// Fails with `UnknownMode` for a mode nobody registered, `InvalidUtf8`
        /// for text that is not text, and `EmptyText` when there is nothing to
        /// type: a run with no text is over before it starts, and saying so is
        /// better than presenting an empty screen.
        [[nodiscard]] core::Result<ActiveRun> start(const SessionRequest& request) const;

        /// Computes every metric from the log and writes the run.
        ///
        /// One `save_run`, one transaction: the session, its per-second
        /// samples, the personal bests it set and the lifetime key, bigram and
        /// error-pair totals land together or not at all. A partial write here
        /// would be silently wrong forever after, because the obvious repair —
        /// merging the stats again — would double-count the run that did land.
        [[nodiscard]] core::Result<SessionResult> finish(const ActiveRun& run, Outcome outcome) const;

    private:
        IHistoryRepository* history_;
        const core::ModeRegistry* modes_;
        const core::IClock* clock_;
        const core::IWallClock* wall_clock_;
    };

}  // namespace typeit::app

#endif  // TYPEIT_APP_SERVICES_SESSIONSERVICE_H
