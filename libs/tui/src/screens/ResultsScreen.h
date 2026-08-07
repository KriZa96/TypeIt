// What the run came to (TI-093, TI-105).
//
// The headline numbers, the per-second chart, how this run compares with the
// others, what went wrong most often, and the three things somebody does next.
//
// It renders a `SessionResult` — the same values that were written to the
// database — rather than reaching back into the session. A results screen that
// recomputed would be a second implementation of the metrics, and the two would
// disagree eventually.
//
// **Comparisons are absent rather than wrong.** With no history behind it there
// is nothing to compare against, and a line reading "+0 vs average" on somebody's
// first run is worse than no line: it invents a baseline out of the run itself.
#ifndef TYPEIT_TUI_SCREENS_RESULTSSCREEN_H
#define TYPEIT_TUI_SCREENS_RESULTSSCREEN_H

#include <cstdint>
#include <ftxui/component/event.hpp>
#include <ftxui/dom/elements.hpp>
#include <optional>
#include <string_view>
#include <utility>

#include "IScreen.h"
#include "ScreenContext.h"
#include "typeit/app/records/History.h"
#include "typeit/app/services/SessionService.h"

namespace typeit::tui {

    /// What the screen was asked to do. Read and cleared by whoever is driving
    /// the stack — the screen does not push anything itself, because a screen
    /// that navigated would need to know about every other screen.
    enum class ResultsAction : std::uint8_t {
        Restart,
        NewText,
        Menu,
    };

    /// What the other runs in this mode came to. One optional around both,
    /// not one each: they are read from the same query and are always either
    /// both known or both unknown, and two optionals would be an invariant
    /// nothing enforces.
    struct Baseline {
        core::Wpm mean_net_wpm{0.0};
        core::Wpm best_net_wpm{0.0};
    };

    /// The "how did that compare" block. An absent baseline means there was
    /// nothing to compare against, which is a different thing from zero.
    struct Comparison {
        std::optional<Baseline> baseline;
        /// True when this run beat every one before it. Announced distinctly,
        /// because it is the only line here somebody wants to see.
        bool personal_best = false;
    };

    class ResultsScreen : public IScreen {
    public:
        /// The parity constructor: a record and nothing else. Still here
        /// because a run started before there was any history to compare it
        /// with is a normal thing to have finished.
        ResultsScreen(const ScreenContext& context, app::SessionRecord record) :
            context_{&context},
            result_{.id = core::SessionId{0}, .record = std::move(record), .keys = {}, .errors = {}} {
            load_comparison();
        }

        /// The full version: everything `finish` produced, so the pair lists
        /// come from the numbers that were written rather than from a second
        /// pass over a log this screen does not have.
        ResultsScreen(const ScreenContext& context, app::SessionResult result) :
            context_{&context}, result_{std::move(result)} {
            load_comparison();
        }

        [[nodiscard]] const Comparison& comparison() const noexcept { return comparison_; }

        [[nodiscard]] ftxui::Element render() override;
        [[nodiscard]] bool on_event(ftxui::Event event) override;
        [[nodiscard]] std::string_view title() const override { return "results"; }

        /// The action asked for since the last time this was read, if any.
        [[nodiscard]] std::optional<ResultsAction> take_action();

    private:
        /// Reads the history once, when the screen opens. Never in `render`.
        void load_comparison();

        const ScreenContext* context_;
        app::SessionResult result_;
        Comparison comparison_;
        std::optional<ResultsAction> requested_;
    };

}  // namespace typeit::tui

#endif  // TYPEIT_TUI_SCREENS_RESULTSSCREEN_H
