//
// Created by kil3 on 3/14/25.
//

#include "../../include/view/Main.h"
#include "../../include/data/GameState.h"

Main::Main():
    menu_(screen.get_screen_reference()),
    speed_typing_session_(std::make_shared<SpeedTypingSession>()),
    speed_typing_session_component_(speed_typing_session_->get_speed_typing_session_component()),
    menu_component_(menu_.get_menu_component()),
    container_(ftxui::Container::Stacked(
            {
                ftxui::Maybe(menu_component_, [&] {return not GameState::game_session_in_progress_;}),
                ftxui::Maybe(speed_typing_session_component_, &GameState::game_session_in_progress_)
            }
        )
    ) {}

void Main::start() {
    const ftxui::Component main_component = get_main_component();
    screen.loop(main_component);
}

void Main::refresh_game_session() {
    speed_typing_session_ = std::make_shared<SpeedTypingSession>();
    speed_typing_session_component_ = speed_typing_session_->get_speed_typing_session_component();
    container_->DetachAllChildren();
    container_->Add(
        ftxui::Maybe(menu_component_, [&] {return not GameState::game_session_in_progress_;})
    );
    container_->Add(
        ftxui::Maybe(speed_typing_session_component_, &GameState::game_session_in_progress_)
    );
}

ftxui::Component Main::get_main_component() {
    return ftxui::Renderer(
        container_,
        [this] {
            if (GameState::refresh_session_) {
                GameState::refresh_session_ = false;
                GameState::game_session_in_progress_ = true;
                refresh_game_session();
            }
            return container_->Render();
        }
    );
}