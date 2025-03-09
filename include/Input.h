//
// Created by kil3 on 3/8/25.
//

#ifndef INPUT_H
#define INPUT_H

#include "ftxui/component/component.hpp"

#include "Text.h"


class Input {
    public:
        Input(const int text_lines_size,
            std::shared_ptr<std::string> text,
            std::shared_ptr<std::vector<std::string>> text_lines,
            std::shared_ptr<Text> text_instance
        );
        ftxui::Component get_input_component();
    private:
        int current_line_index_;
        std::string input_text_;
        ftxui::Component input_component_;
        std::shared_ptr<Text> text_instance_;
        ftxui::Elements current_input_line_;
        std::vector<ftxui::Elements> previous_input_lines_;
        ftxui::Elements total_input_lines_;

        void render_input_text();
        void add_element();
        void remove_element();
        void go_to_new_line();
        void go_to_previous_line();
        ftxui::Element get_next_character(const int character_index) const;
};



#endif //INPUT_H
