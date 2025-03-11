//
// Created by kil3 on 3/8/25.
//

#ifndef INPUT_H
#define INPUT_H

#include "ftxui/component/component.hpp"

#include "Text.h"


class Input {
    public:
        explicit Input(std::shared_ptr<Text> text_instance);
        ftxui::Component get_input_component();
        [[nodiscard]] const int& get_word_count_reference() const;
    private:
        int word_count_;
        int current_line_index_;
        std::string input_text_;
        ftxui::Component input_component_;
        std::shared_ptr<Text> text_instance_;
        ftxui::Elements current_input_line_;
        std::vector<ftxui::Elements> previous_input_lines_;
        ftxui::Elements total_input_lines_;

        void add_element();
        void remove_element();
        void go_to_new_line();
        void render_input_text();
        void go_to_previous_line();
        void set_amount_of_words();
        [[nodiscard]] bool should_go_to_previous_line() const;
        [[nodiscard]] bool Input::should_go_to_next_line() const;
        [[nodiscard]] int get_previous_lines_size() const;
        [[nodiscard]] ftxui::Element get_next_character() const;
};



#endif //INPUT_H
