// Choosing what to type (TI-091, UX §3.1).
//
// The selection lives here, in the screen, and nowhere else. 1.0 keeps it in a
// shared `GameOptions` that three classes write to; the whole point of the
// stack is that a screen's state ends when the screen does.
//
// **Validation happens on change, not while rendering.** 1.0 runs `std::stoi`
// in a `try`/`catch` with an *empty catch block*, inside a render callback, on
// every frame — so a bad value is silently swallowed sixty times a second and
// the user is told nothing. Here a keystroke validates once and the message
// stays on screen until the value is fixed.
#ifndef TYPEIT_TUI_SCREENS_MENUSCREEN_H
#define TYPEIT_TUI_SCREENS_MENUSCREEN_H

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

namespace typeit::tui {

    /// The controls, in tab order. The order is the enum's, so it cannot drift
    /// from what the screen draws.
    enum class MenuField : std::uint8_t {
        Mode,
        Duration,
        WordCount,
        Text,
        Start,
    };

    inline constexpr std::array<MenuField, 5> kMenuFields{MenuField::Mode, MenuField::Duration, MenuField::WordCount,
                                                          MenuField::Text, MenuField::Start};

    /// What the menu decided, handed to whoever starts the run.
    struct MenuSelection {
        std::string mode = "timed";
        std::int64_t seconds = 30;
        std::int64_t words = 50;

        /// Which entry of the context's catalogue. One past the last means the
        /// path below, which is how 1.0's fourth radio button worked too.
        std::size_t text = 0;
        std::string custom_path;
    };

    class MenuScreen : public IScreen {
    public:
        explicit MenuScreen(const ScreenContext& context);

        [[nodiscard]] ftxui::Element render() override;
        [[nodiscard]] bool on_event(ftxui::Event event) override;
        [[nodiscard]] std::string_view title() const override { return "menu"; }

        [[nodiscard]] const MenuSelection& selection() const noexcept { return selection_; }
        [[nodiscard]] MenuField focused() const noexcept { return focused_; }

        /// What is wrong with the selection, or empty. Set when a value
        /// changes and when starting is refused — never inside `render`.
        [[nodiscard]] const std::string& message() const noexcept { return message_; }

        /// True once the user has asked to start with a selection that
        /// validates. Read and cleared by the caller.
        [[nodiscard]] bool take_start();

    private:
        void focus_next();
        void focus_previous();
        /// Enter: validates, and says why if it will not start.
        void request_start();
        /// One printable character, into whichever field wants it.
        [[nodiscard]] bool typed(char letter);
        /// Left or right on a list-valued field.
        [[nodiscard]] bool cycle(bool forward);
        /// Applies a digit or a deletion to the focused numeric field, and
        /// validates the result **now**.
        void edit(char digit);
        void backspace_field();
        [[nodiscard]] bool validate();

        /// How many texts the catalogue offers. The index one past the last is
        /// "a path I type myself", so the field always has one more position
        /// than there are entries.
        [[nodiscard]] std::size_t text_choices() const;
        [[nodiscard]] bool typing_a_path() const;

        const ScreenContext* context_;
        MenuSelection selection_;
        MenuField focused_ = MenuField::Mode;
        std::string message_;
        bool start_requested_ = false;
        /// The digits typed into the focused field, so a half-typed number is
        /// not repeatedly parsed as a whole one.
        std::string editing_;
    };

}  // namespace typeit::tui

#endif  // TYPEIT_TUI_SCREENS_MENUSCREEN_H
