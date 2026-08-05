// One run, assembled (ARCHITECTURE section 5.2).
//
// The four things a run is made of — the text, the model over it, the mode
// that decides when it ends, and the provider the text came from — held
// together with the loop that keeps them in step. Nothing else: no clock, no
// FTXUI, no database. Time arrives as a parameter, exactly as it does to
// `TypingModel` and `IMode`, and persistence happens above this in
// `app::SessionService`.
//
// That is what makes `typeit --simulate` the same code path as a real run
// rather than a second implementation of it: a scripted session and a typed
// one both come through `on_key` and `on_tick` and differ only in where the
// timestamps come from.
//
// The wiring is small but it is the part that is easy to get subtly wrong: a
// keystroke a rule refused must not be reported to the mode, because it never
// happened, and a mode told about it would count a word the typist did not
// type. `on_key` asks the log whether anything was recorded rather than
// assuming.
#ifndef TYPEIT_CORE_SESSION_SESSION_H
#define TYPEIT_CORE_SESSION_SESSION_H

#include <memory>

#include "typeit/core/modes/IMode.h"
#include "typeit/core/session/TypingModel.h"
#include "typeit/core/session/TypingRules.h"
#include "typeit/core/text/Grapheme.h"
#include "typeit/core/text/TextBuffer.h"
#include "typeit/core/text_supply/ITextProvider.h"
#include "typeit/core/util/Result.h"
#include "typeit/core/util/Units.h"

namespace typeit::core {

    class Session {
    public:
        /// What a run is built from. Both are owned: a mode outlives no run,
        /// and a provider that was borrowed could be drained by somebody else
        /// halfway through.
        struct Parts {
            std::unique_ptr<IMode> mode;
            std::unique_ptr<ITextProvider> provider;
            TypingRules rules;
        };

        /// Takes the provider's first chunk, builds the text, and starts the
        /// mode on it.
        ///
        /// Fails with `ErrorCode::InvalidUtf8`, naming the byte, if the
        /// provider produced something that is not text. An empty chunk is not
        /// an error here: a run with nothing to type is a caller's problem to
        /// reject, and `app::SessionService` does so with a message this layer
        /// could not write.
        ///
        /// Returned by pointer because `TypingModel` holds a span into the
        /// buffer this owns. A moved-from session would leave that span
        /// pointing at a corpse, so the type is immovable and the only handle
        /// to it is one that does not move either.
        ///
        /// `started_at` is on the monotonic clock, like every other timestamp
        /// a session sees. The wall-clock date a run is filed under is
        /// `app`'s to record and never reaches here.
        [[nodiscard]] static Result<std::unique_ptr<Session>> create(Parts parts, Millis started_at);

        ~Session() = default;
        Session(const Session&) = delete;
        Session& operator=(const Session&) = delete;
        Session(Session&&) = delete;
        Session& operator=(Session&&) = delete;

        /// Applies one grapheme and tells the mode about it — but only if the
        /// model recorded it. A keystroke the rules refused, or one typed past
        /// the end of the text, logs nothing and is not an event.
        void on_key(const Grapheme& grapheme, Millis at);

        /// The same, for a deletion. A backspace at the start of the text is
        /// likewise a non-event.
        void on_backspace(Millis at);

        /// Time has passed. The mode needs this whether or not anything was
        /// typed: a timed run ends while the typist stares at the screen.
        void on_tick(Millis now);

        [[nodiscard]] bool is_finished() const { return mode_->is_finished(); }
        [[nodiscard]] ModeProgress progress() const { return mode_->progress(); }

        [[nodiscard]] const TypingModel& model() const noexcept { return model_; }
        [[nodiscard]] const TextBuffer& text() const noexcept { return text_; }
        [[nodiscard]] const IMode& mode() const noexcept { return *mode_; }
        [[nodiscard]] const ITextProvider& provider() const noexcept { return *provider_; }

        /// When the run was offered, on the monotonic clock. Not when the first
        /// key was pressed — that is a mode's business, and `TimedMode` reports
        /// it separately precisely because the two are different numbers.
        [[nodiscard]] Millis started_at() const noexcept { return started_at_; }

    private:
        Session(TextBuffer text, Parts parts, Millis started_at);

        /// Reports the last logged event to the mode when `before` says one was
        /// just added.
        void notify_if_logged(std::size_t before);

        // Declaration order is load-bearing: `model_` holds a span into
        // `text_`, so the text must be built first and destroyed last.
        TextBuffer text_;
        std::unique_ptr<IMode> mode_;
        std::unique_ptr<ITextProvider> provider_;
        TypingModel model_;
        Millis started_at_;
    };

}  // namespace typeit::core

#endif  // TYPEIT_CORE_SESSION_SESSION_H
