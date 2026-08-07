// Everything that has been recorded, on one screen (TI-103, UX §3.5).
//
// **Nothing is queried in `render`.** The database is asked once when the
// screen opens and again when a filter changes, and drawing reads what came
// back. A query inside a render callback runs sixty times a second against a
// file on disk, which is the same defect as 1.0's `std::stoi` in a render
// transform wearing a much more expensive hat.
//
// A failed query is not a crash and not a blank screen: whatever could not be
// read is reported in place, and the parts that did read still draw. A history
// screen that goes blank because one aggregate failed tells its reader nothing
// about the runs it *can* see.
#ifndef TYPEIT_TUI_SCREENS_HISTORYSCREEN_H
#define TYPEIT_TUI_SCREENS_HISTORYSCREEN_H

#include <array>
#include <cstddef>
#include <cstdint>
#include <ftxui/component/event.hpp>
#include <ftxui/dom/elements.hpp>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "IScreen.h"
#include "ScreenContext.h"
#include "typeit/app/records/History.h"
#include "typeit/app/services/HistoryService.h"
#include "typeit/core/metrics/KeyStats.h"
#include "typeit/core/util/Units.h"

namespace typeit::tui {

    /// The ranges the screen offers, in the order the filter cycles through
    /// them. `All` first, because that is what the screen opens with and what
    /// somebody with three runs wants to see.
    enum class DateRange : std::uint8_t {
        All,
        Week,
        Month,
        Year,
    };

    inline constexpr std::array<DateRange, 4> kAllDateRanges{DateRange::All, DateRange::Week, DateRange::Month,
                                                             DateRange::Year};

    [[nodiscard]] std::string_view to_string(DateRange range);

    /// How many days a range covers, or zero for `All`.
    [[nodiscard]] std::int64_t days_in(DateRange range);

    /// Which control the keyboard is on.
    enum class HistoryField : std::uint8_t {
        Mode,
        Range,
        Sessions,
    };

    /// Everything one query round produced, so drawing reads a value rather
    /// than reaching for a database.
    struct HistoryData {
        app::Aggregates totals;
        app::Streak streak;
        std::vector<app::TrendPoint> trend;
        std::vector<app::PersonalBest> bests;
        std::vector<app::SessionRow> sessions;
        core::KeyStats keys;
        /// What went wrong, if anything. Shown in place of the part that failed
        /// rather than instead of the screen.
        std::vector<std::string> problems;
    };

    class HistoryScreen : public IScreen {
    public:
        /// Loads immediately, so the screen is never drawn before it has
        /// anything to draw.
        explicit HistoryScreen(const ScreenContext& context);

        [[nodiscard]] ftxui::Element render() override;
        [[nodiscard]] bool on_event(ftxui::Event event) override;
        [[nodiscard]] std::string_view title() const override { return "history"; }

        [[nodiscard]] const HistoryData& data() const noexcept { return data_; }
        [[nodiscard]] const app::HistoryFilter& filter() const noexcept { return filter_; }
        [[nodiscard]] HistoryField focused() const noexcept { return focused_; }
        [[nodiscard]] std::size_t selected() const noexcept { return selected_; }

        /// The session the user asked to look at, read and cleared by whoever
        /// drives the stack. The screen pushes nothing itself — that is
        /// TI-081's rule and it is what keeps navigation in one place.
        [[nodiscard]] std::optional<core::SessionId> take_opened();

        /// How many rows fit at the current terminal size. A page, for the
        /// paging keys, and worth reading in a test rather than inferring.
        [[nodiscard]] std::size_t page_size() const;

    private:
        void reload();
        void cycle_mode(bool forward);
        void cycle_range(bool forward);
        void move_selection(std::int64_t by);

        const ScreenContext* context_;
        app::HistoryFilter filter_;
        DateRange range_ = DateRange::All;
        HistoryData data_;
        HistoryField focused_ = HistoryField::Mode;
        std::size_t selected_ = 0;
        /// The first row drawn, so a long history scrolls rather than drawing
        /// its first screenful forever.
        std::size_t first_row_ = 0;
        std::optional<core::SessionId> opened_;
    };

}  // namespace typeit::tui

#endif  // TYPEIT_TUI_SCREENS_HISTORYSCREEN_H
