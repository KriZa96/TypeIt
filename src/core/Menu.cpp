//
// Created by Kristijan Zalac on 3/12/25.
//

#include "../../include/core/Menu.h"

#include "../../include/core/Screen.h"
#include "../../include/data/GameOptions.h"
#include "../../include/data/GameState.h"
#include "../../include/data/Style.h"
#include "../../include/data/ComponentOptions.h"


Menu::Menu(ftxui::ScreenInteractive& screen):
screen_(screen),
text_radiobox_choice_({"simple", "medium", "hard", "custom"}),
time_radiobox_choice_({"15s", "30s", "60s", "custom"}),
path_input_(ftxui::Input(&GameOptions::text_radiobox_values_[3], "Path to file", ComponentOptions::menu_path_input_option)),
time_input_(ftxui::Input(&GameOptions::custom_radiobox_input_, "Input seconds", ComponentOptions::menu_time_input_option)),
time_radiobox_(ftxui::Radiobox(&time_radiobox_choice_, &GameOptions::selected_radiobox_time_, ComponentOptions::menu_radiobox_option)),
text_radiobox_(ftxui::Radiobox(&text_radiobox_choice_, &GameOptions::selected_radiobox_text_, ComponentOptions::menu_radiobox_option)),
exit_button_(ftxui::Button("Exit", [this] {exit_application();}, ftxui::ButtonOption::Ascii())),
info_button_(ftxui::Button("Info", [this] {GameState::toggle_info_display();}, ftxui::ButtonOption::Ascii())),
start_button_(ftxui::Button("Start", [this] {GameState::start_game_session();}, ftxui::ButtonOption::Ascii())),
menu_container_(get_menu_container_()) {}


void Menu::exit_application() const {
    screen_.ExitLoopClosure();
    screen_.Exit();
}


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


ftxui::Component Menu::get_info_component_() const {
    return ftxui::Maybe(ftxui::Renderer(
        [&] {
            return ftxui::window(
                ftxui::text("Info"),
                ftxui::paragraph(
                "Custom inputs will change color from "
                    "salmon to grey/white, on valid input."
                )
            );
        }
    ), &GameState::show_info);
}


ftxui::Component Menu::get_menu_container_() const {
    return ftxui::Container::Vertical(
            {
                get_info_component_(),
                ftxui::Container::Horizontal({info_button_}) | ftxui::center,
                get_text_radiobox_component_(),
                ftxui::Maybe(path_input_, [&] {return GameOptions::selected_radiobox_text_ == 3;}),
                get_time_radiobox_component_(),
                ftxui::Maybe(time_input_, [&] {return GameOptions::selected_radiobox_time_ == 3;}),
                ftxui::Container::Horizontal(
                    {
                        start_button_,
                        exit_button_
                    }
                ) | ftxui::center
            }
        ) | Style::menu_style;
}
