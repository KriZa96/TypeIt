// What the run came to (TI-093).
//
// The headline numbers and the three things somebody does next. Charts are
// Phase 5; this is parity with 1.0 and no more, because merging the cutover
// with new features is how a migration stalls.
//
// It renders a `SessionRecord` — the same value that was written to the
// database — rather than reaching back into the session. A results screen that
// recomputed would be a second implementation of the metrics, and the two
// would disagree eventually.
#ifndef TYPEIT_TUI_SCREENS_RESULTSSCREEN_H
#define TYPEIT_TUI_SCREENS_RESULTSSCREEN_H

#include <cstdint>
#include <ftxui/component/event.hpp>
#include <ftxui/dom/elements.hpp>
#include <optional>
#include <string_view>

#include "IScreen.h"
#include "ScreenContext.h"
#include "typeit/app/records/History.h"

namespace typeit::tui {

    /// What the screen was asked to do. Read and cleared by whoever is driving
    /// the stack — the screen does not push anything itself, because a screen
    /// that navigated would need to know about every other screen.
    enum class ResultsAction : std::uint8_t {
        Restart,
        NewText,
        Menu,
    };

    class ResultsScreen : public IScreen {
    public:
        ResultsScreen(const ScreenContext& context, app::SessionRecord record) :
            context_{&context}, record_{std::move(record)} {}

        [[nodiscard]] ftxui::Element render() override;
        [[nodiscard]] bool on_event(ftxui::Event event) override;
        [[nodiscard]] std::string_view title() const override { return "results"; }

        /// The action asked for since the last time this was read, if any.
        [[nodiscard]] std::optional<ResultsAction> take_action();

    private:
        const ScreenContext* context_;
        app::SessionRecord record_;
        std::optional<ResultsAction> requested_;
    };

}  // namespace typeit::tui

#endif  // TYPEIT_TUI_SCREENS_RESULTSSCREEN_H
