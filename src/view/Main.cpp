//
// Created by kil3 on 3/14/25.
//

#include "../../include/view/Main.h"

#include "../../include/data/FocusPosition.h"
#include "../../include/data/GameState.h"

Main::Main():
    menu_(screen.get_screen_reference()),
    speed_typing_session_(std::make_shared<SpeedTypingSession>()),
    speed_typing_session_component_(speed_typing_session_->get_speed_typing_session_component()),
    menu_component_(menu_.get_menu_component()),
    container_(
        ftxui::Container::Stacked({get_main_component_maybe(), get_speed_typing_session_component_maybe()})
    ) {}

void Main::Start() {
    Main main;
    main.start_main_session();
}

void Main::start_main_session() {
    const ftxui::Component main_component = get_main_component();
    screen.loop(main_component);
}

ftxui::Component Main::get_main_component_maybe() const {
    return ftxui::Maybe(menu_component_, [&] {return !GameState::game_session_in_progress;});
}
ftxui::Component Main::get_speed_typing_session_component_maybe() const {
    return ftxui::Maybe(speed_typing_session_component_, &GameState::game_session_in_progress);
}

void Main::refresh_game_session() {
    speed_typing_session_ = std::make_shared<SpeedTypingSession>();
    speed_typing_session_component_ = speed_typing_session_->get_speed_typing_session_component();
    container_->DetachAllChildren();
    container_->Add(get_main_component_maybe());
    container_->Add(get_speed_typing_session_component_maybe());
}

ftxui::Component Main::get_main_component() {
    return ftxui::Renderer(
        container_,
        [this] {
            if (GameState::refresh_session || GameState::start_session) {
                FocusPosition::reset();
                GameState::game_finished = false;
                GameState::start_session = false;
                GameState::refresh_session = false;
                GameState::game_session_in_progress = true;
                refresh_game_session();
            }
            return container_->Render();
        }
    );
}