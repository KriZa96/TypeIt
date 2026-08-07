// Everything imported, on one screen (TI-118, TI-112, UX §3.6).
//
// The listing, and the four things somebody does to it: import a file, paste
// something, tag it, delete it. Search narrows as you type.
//
// **Nothing is queried in `render`**, same as the history screen: the library
// is read when the screen opens and after anything changes it, and drawing
// reads a member.
//
// **Deleting confirms, and says what else goes.** It is the only thing here
// that destroys anything, and "deleted" alone leaves somebody wondering about
// the runs they typed from it.
#ifndef TYPEIT_TUI_SCREENS_TEXTLIBRARYSCREEN_H
#define TYPEIT_TUI_SCREENS_TEXTLIBRARYSCREEN_H

#include <cstddef>
#include <cstdint>
#include <ftxui/component/event.hpp>
#include <ftxui/dom/elements.hpp>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "ColorQuantizer.h"
#include "IScreen.h"
#include "ScreenContext.h"
#include "typeit/app/records/TextLibrary.h"
#include "typeit/app/services/TextLibraryService.h"
#include "typeit/core/util/Units.h"

namespace typeit::tui {

    /// What the screen is doing. A mode rather than four booleans: exactly one
    /// prompt is open at a time, and two booleans could both be true.
    enum class LibraryMode : std::uint8_t {
        /// Moving through the list.
        Browsing,
        /// Typing into the search box, which filters as it goes.
        Searching,
        /// Typing a path to import.
        Importing,
        /// Typing, or pasting, the text itself.
        Pasting,
        /// Typing a tag for the selected text.
        Tagging,
        /// Answering "really delete this?".
        Confirming,
    };

    /// One row, with the progress the bookmark says.
    struct LibraryRow {
        app::TextSummary summary;
        app::TextProgress progress;
    };

    class TextLibraryScreen : public IScreen {
    public:
        explicit TextLibraryScreen(const ScreenContext& context);

        [[nodiscard]] ftxui::Element render() override;
        [[nodiscard]] bool on_event(ftxui::Event event) override;
        [[nodiscard]] std::string_view title() const override { return "text library"; }

        [[nodiscard]] const std::vector<LibraryRow>& rows() const noexcept { return rows_; }
        [[nodiscard]] LibraryMode mode() const noexcept { return mode_; }
        [[nodiscard]] std::size_t selected() const noexcept { return selected_; }
        [[nodiscard]] const std::string& typed() const noexcept { return typed_; }
        [[nodiscard]] const std::string& message() const noexcept { return message_; }

        /// The text the user chose to type, read and cleared by whoever drives
        /// the stack. The screen pushes nothing itself.
        [[nodiscard]] std::optional<core::TextId> take_chosen();

    private:
        void reload();
        void move_selection(std::int64_t by);
        /// Whichever prompt is open, given one character.
        [[nodiscard]] bool handle_prompt(const ftxui::Event& event);
        /// Enter, in whichever prompt is open. One member per action rather
        /// than one switch that does all four: they share only the moment they
        /// happen at.
        void commit();
        void do_import();
        void do_paste();
        void do_tag();
        void do_delete();
        void begin(LibraryMode mode);

        [[nodiscard]] ftxui::Element list_rows(const Styling& accent, const Styling& muted) const;
        [[nodiscard]] ftxui::Element prompt_row(const Styling& accent, const Styling& muted) const;

        const ScreenContext* context_;
        std::vector<LibraryRow> rows_;
        std::string search_;
        /// What has been typed into whichever prompt is open. One buffer rather
        /// than one per mode: only one prompt exists at a time, and four
        /// buffers would be three ways to show somebody yesterday's typing.
        std::string typed_;
        std::string message_;
        LibraryMode mode_ = LibraryMode::Browsing;
        std::size_t selected_ = 0;
        std::size_t first_row_ = 0;
        std::optional<core::TextId> chosen_;
    };

}  // namespace typeit::tui

#endif  // TYPEIT_TUI_SCREENS_TEXTLIBRARYSCREEN_H
