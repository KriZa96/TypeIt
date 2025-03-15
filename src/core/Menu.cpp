//
// Created by kil3 on 3/12/25.
//

#include "../../include/core/Menu.h"
#include "../../include/core/Screen.h"
#include "../../include/data/GameState.h"
#include "../../include/data/GameOptions.h"


Menu::Menu(ftxui::ScreenInteractive& screen):
screen_(screen),
text_radiobox_choice_({"simple", "medium", "hard", "custom"}),
time_radiobox_choice_({"15s", "30s", "60s"}),
time_radiobox_(ftxui::Radiobox(&time_radiobox_choice_, &GameOptions::selected_radiobox_time_, radiobox_option_)),
text_radiobox_(ftxui::Radiobox(&text_radiobox_choice_, &GameOptions::selected_radiobox_text_, radiobox_option_)),
start_button_(ftxui::Button("Start", [this] {GameState::refresh_session_ = not GameState::refresh_session_;}, ftxui::ButtonOption::Ascii())),
exit_button_(ftxui::Button("Exit", [this] {screen_.ExitLoopClosure(); screen_.Exit();}, ftxui::ButtonOption::Ascii())),
menu_container_(get_menu_container_()),
radiobox_option_({
    .transform = [&](const ftxui::EntryState& state) {
        #if defined(FTXUI_MICROSOFT_TERMINAL_FALLBACK)
            auto prefix = ftxui::text(state.state ? "(*) " : "( ) ") | ftxui::color(ftxui::Color::Grey85);
        #else
            auto prefix = ftxui::text(state.state ? "◉ " : "○ ") | ftxui::color(ftxui::Color::Grey85);
        #endif
            auto t = ftxui::text(state.label) | ftxui::color(ftxui::Color::Grey85);
            if (state.focused) {
                t |= ftxui::inverted;
            }
            return ftxui::hbox({prefix, t});
    }
}) {}



ftxui::Component Menu::get_menu_component() const {
    return ftxui::Renderer(
            menu_container_,
            [this] {
                return menu_container_->Render();
            }
    );
}

ftxui::Component Menu::get_time_radiobox_component_() const {
    return ftxui::Renderer(
        time_radiobox_,
        [this] {
            return ftxui::window(ftxui::text("Time"), time_radiobox_->Render());
        }
    );
}

ftxui::Component Menu::get_text_radiobox_component_() const {
    return ftxui::Renderer(
        text_radiobox_,
        [this] {
            return ftxui::window(ftxui::text("Text"), text_radiobox_->Render());
        }
    );
}

ftxui::Component Menu::get_menu_container_() const {
    return ftxui::Container::Vertical(
            {
                get_text_radiobox_component_(),
                get_time_radiobox_component_(),
                ftxui::Container::Horizontal(
                    {
                        start_button_,
                        exit_button_
                    }
                ) | ftxui::center
            }
        ) | ftxui::size(ftxui::WIDTH, ftxui::EQUAL, 15) | ftxui::size(ftxui::HEIGHT, ftxui::EQUAL, 20) | ftxui::center;
}