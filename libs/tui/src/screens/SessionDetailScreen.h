// One past run, in full (TI-104, UX §3.5).
//
// Read once when the screen opens, like everything else that touches the
// database. What it shows that the list cannot is the per-second chart, which
// is the whole reason the samples are stored.
//
// **A run recorded by an older major version is shown with a note.** Metric
// definitions may differ across a MAJOR version (VERSIONING §9) — 2.0 changed
// what WPM and accuracy mean — so a 1.x run's numbers are not comparable with
// today's, and displaying them side by side without saying so is the quiet kind
// of wrong.
#ifndef TYPEIT_TUI_SCREENS_SESSIONDETAILSCREEN_H
#define TYPEIT_TUI_SCREENS_SESSIONDETAILSCREEN_H

#include <ftxui/component/event.hpp>
#include <ftxui/dom/elements.hpp>
#include <optional>
#include <string>
#include <string_view>

#include "IScreen.h"
#include "ScreenContext.h"
#include "typeit/app/records/History.h"
#include "typeit/core/util/Units.h"

namespace typeit::tui {

    /// Whether a stored run's numbers mean what today's mean.
    ///
    /// A string comparison on the major version rather than a parse: the field
    /// is free text written by whatever wrote the row, and a version this
    /// cannot read is exactly the case that deserves the note.
    [[nodiscard]] bool recorded_by_this_major(std::string_view app_version);

    class SessionDetailScreen : public IScreen {
    public:
        SessionDetailScreen(const ScreenContext& context, core::SessionId id);

        [[nodiscard]] ftxui::Element render() override;
        [[nodiscard]] bool on_event(ftxui::Event event) override;
        [[nodiscard]] std::string_view title() const override { return "session detail"; }

        /// The run, once it has been read. Empty when the read failed, which
        /// the screen says rather than drawing a page of zeros.
        [[nodiscard]] const std::optional<app::SessionRecord>& record() const noexcept { return record_; }
        [[nodiscard]] const std::string& problem() const noexcept { return problem_; }

        /// True once the user has asked to go back. Read and cleared by
        /// whoever drives the stack.
        [[nodiscard]] bool take_back();

    private:
        const ScreenContext* context_;
        std::optional<app::SessionRecord> record_;
        std::string problem_;
        bool back_ = false;
    };

}  // namespace typeit::tui

#endif  // TYPEIT_TUI_SCREENS_SESSIONDETAILSCREEN_H
