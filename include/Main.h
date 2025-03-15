//
// Created by kil3 on 3/14/25.
//

#ifndef MAIN_H
#define MAIN_H

#include "ftxui/component/component.hpp"

#include "./Screen.h"
#include "./Menu.h"
#include "./SpeedTypingSession.h"
#include "./GameOptions.h"

class Main {
    public:
        Main():
            menu_(screen.get_screen_reference()),
            speed_typing_session_(std::make_shared<SpeedTypingSession>()),
            speed_typing_session_component_(speed_typing_session_->get_speed_typing_session_component()),
            menu_component_(menu_.get_menu_component()),
            container_(ftxui::Container::Stacked(
                    {
                        ftxui::Maybe(menu_component_, [&] {return not GameOptions::game_session_in_progress_;}),
                        ftxui::Maybe(speed_typing_session_component_, &GameOptions::game_session_in_progress_)
                    }
                )
            ) {}

        void start() {
            const ftxui::Component main_component = get_main_component();
            screen.loop(main_component);
        }

    private:
        Screen screen;
        Menu menu_;
        std::shared_ptr<SpeedTypingSession> speed_typing_session_;

        ftxui::Component speed_typing_session_component_;
        ftxui::Component menu_component_;
        ftxui::Component container_;

        void refresh_game_session() {
            speed_typing_session_ = std::make_shared<SpeedTypingSession>();
            speed_typing_session_component_ = speed_typing_session_->get_speed_typing_session_component();
            container_->DetachAllChildren();
            container_->Add(
                ftxui::Maybe(menu_component_, [&] {return not GameOptions::game_session_in_progress_;})
            );
            container_->Add(
                ftxui::Maybe(speed_typing_session_component_, &GameOptions::game_session_in_progress_)
            );
        }

        ftxui::Component get_main_component() {
            return ftxui::Renderer(
                container_,
                [this] {
                    if (GameOptions::refresh_session_) {
                        GameOptions::refresh_session_ = false;
                        GameOptions::game_session_in_progress_ = true;
                        refresh_game_session();
                    }
                    return container_->Render();
                }
            );
        }
};



#endif //MAIN_H
