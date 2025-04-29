//
// Created by Kristijan Zalac on 3/12/25.
//

#ifndef MENU_H
#define MENU_H

#include "ftxui/component/component.hpp"


class Menu {
    public:
        explicit Menu(ftxui::ScreenInteractive& screen);
        [[nodiscard]] ftxui::Component get_menu_component() const;
    private:
        ftxui::ScreenInteractive& screen_;

        std::vector<std::string> text_radiobox_choice_;
        std::vector<std::string> time_radiobox_choice_;

        ftxui::Component path_input_;
        ftxui::Component time_input_;
        ftxui::Component time_radiobox_;
        ftxui::Component text_radiobox_;
        ftxui::Component start_button_;
        ftxui::Component exit_button_;
        ftxui::Component info_button_;

        ftxui::Component menu_container_;

        void exit_application() const;
        [[nodiscard]] ftxui::Component get_time_radiobox_component_() const;
        [[nodiscard]] ftxui::Component get_text_radiobox_component_() const;
        [[nodiscard]] ftxui::Component get_menu_container_() const;
        [[nodiscard]] ftxui::Component get_info_component_() const;
};



#endif //MENU_H
