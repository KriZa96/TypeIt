// A run, on screen (TI-092).
//
// Wires the typing area, the stats bar and the mode together. It owns the
// session, so restarting means building a new one — not resetting flags on an
// old one, which is what 1.0 does and why a second run there is never quite as
// clean as the first.
//
// **Nothing mutates in `render`.** Keys advance the model, ticks advance the
// mode, and drawing does neither. That split is the whole point of the rewrite
// and there is a test for each half of it.
#ifndef TYPEIT_TUI_SCREENS_SESSIONSCREEN_H
#define TYPEIT_TUI_SCREENS_SESSIONSCREEN_H

#include <cstdint>
#include <ftxui/component/event.hpp>
#include <ftxui/dom/elements.hpp>
#include <memory>
#include <optional>
#include <string_view>

#include "IScreen.h"
#include "ScreenContext.h"
#include "TypingArea.h"
#include "typeit/app/records/History.h"
#include "typeit/app/services/SessionService.h"
#include "typeit/core/util/IClock.h"

namespace typeit::tui {

    /// What the screen wants next. Read by whoever drives the stack; the
    /// screen pushes nothing itself.
    enum class SessionOutcome : std::uint8_t {
        /// The mode said the run is over, and it has been saved.
        Finished,
        /// The user left mid-run. Saved as abandoned, because it happened.
        Abandoned,
        /// The user asked to start again with the same text.
        Restart,
    };

    class SessionScreen : public IScreen {
    public:
        /// Everything is borrowed except the run itself. Fails only if the
        /// service refuses to start — an unknown mode, or nothing to type.
        [[nodiscard]] static core::Result<std::unique_ptr<SessionScreen>> create(const ScreenContext& context,
                                                                                 const app::SessionService& service,
                                                                                 const app::SessionRequest& request,
                                                                                 const core::IClock& clock);

        [[nodiscard]] ftxui::Element render() override;

        /// The race strip and pacer bar, or an empty element for any other

        /// mode (TI-125).

        [[nodiscard]] ftxui::Element race_hud(app::ColorDepth depth, std::size_t width) const;

        [[nodiscard]] bool on_event(ftxui::Event event) override;
        [[nodiscard]] std::string_view title() const override { return "session"; }

        /// Time passing. Advances the mode and nothing else — the frame ticker
        /// calls this, and it must not be able to change what was typed.
        void on_tick(core::Millis now);

        /// The run, once it has one. Empty until the mode finishes or the user
        /// leaves.
        [[nodiscard]] const std::optional<app::SessionResult>& result() const noexcept { return result_; }

        [[nodiscard]] std::optional<SessionOutcome> take_outcome();

        /// Ends the run and saves it as abandoned. What quitting does.
        void abandon();

        /// Seconds of countdown left, or zero once typing may begin. A run
        /// does not start until this reaches zero — but the *timer* still
        /// starts on the first keystroke, which is a different clock.
        [[nodiscard]] std::int64_t countdown() const noexcept { return countdown_left_; }

        [[nodiscard]] const core::Session& session() const noexcept { return *run_.session; }

    private:
        SessionScreen(const ScreenContext& context, const app::SessionService& service, app::ActiveRun run,
                      const core::IClock& clock);

        void finish(app::Outcome outcome, SessionOutcome reported);

        const ScreenContext* context_;
        const app::SessionService* service_;
        app::ActiveRun run_;
        std::shared_ptr<TypingArea> area_;

        std::optional<app::SessionResult> result_;
        std::optional<SessionOutcome> outcome_;
        std::int64_t countdown_left_ = 0;
        core::Millis started_ticking_{0};
    };

}  // namespace typeit::tui

#endif  // TYPEIT_TUI_SCREENS_SESSIONSCREEN_H
