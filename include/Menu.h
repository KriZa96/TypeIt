//
// Created by kil3 on 3/12/25.
//

#ifndef MENU_H
#define MENU_H

#include "ftxui/component/component.hpp"

#include "./GameOptions.h"


class Menu {
    public:
        explicit Menu(ftxui::ScreenInteractive& screen);
        ftxui::Component get_menu_component() const;
    private:
        ftxui::ScreenInteractive& screen_;

        std::vector<std::string> text_radiobox_choice_;
        std::vector<std::string> time_radiobox_choice_;

        ftxui::RadioboxOption radiobox_option_;
        ftxui::Component time_radiobox_;
        ftxui::Component text_radiobox_;
        ftxui::Component start_button_;
        ftxui::Component exit_button_;

        ftxui::Component get_time_radiobox_component_() const;
        ftxui::Component get_text_radiobox_component_() const;
        ftxui::Component get_menu_container_() const;

        ftxui::Component menu_container_;
};



#endif //MENU_H
