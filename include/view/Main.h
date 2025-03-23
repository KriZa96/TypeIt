//
// Created by kil3 on 3/14/25.
//

#ifndef MAIN_H
#define MAIN_H

#include "ftxui/component/component.hpp"
#include "../core/Menu.h"
#include "../core/Screen.h"
#include "SpeedTypingSession.h"

class Main {
    public:
        Main();
        static void Start();
    private:
        Screen screen;
        Menu menu_;
        std::shared_ptr<SpeedTypingSession> speed_typing_session_;

        ftxui::Component speed_typing_session_component_;
        ftxui::Component menu_component_;
        ftxui::Component container_;

        void refresh_game_session();
        void start_main_session();
        ftxui::Component get_main_component();
        ftxui::Component get_main_component_maybe() const;
        ftxui::Component get_speed_typing_session_component_maybe() const;

};



#endif //MAIN_H
