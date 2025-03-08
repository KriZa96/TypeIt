//
// Created by kil3 on 3/8/25.
//

#include "../../include/Text.h"


Text::Text(std::string text, std::shared_ptr<int> position_x, std::shared_ptr<int> position_y):
    text_(text),
    position_x_(position_x),
    position_y_(position_y)    {
    populate_text_lines();
}


void Text::push_line(ftxui::Elements& line, int& num_of_characters){
    text_lines_.push_back(ftxui::hbox(line) | ftxui::color(ftxui::Color::Grey62));
    num_of_characters = 0;
    line.clear();
}


void Text::populate_text_lines() {
    int num_of_characters = 0;
    ftxui::Elements line;

    for (int index = 0; index < text_.size(); index++) {
        if (text_[index] == '\n') {
            push_line(line, num_of_characters);
            continue;
        }

        line.push_back(ftxui::text(std::string(1, text_[index])));
        num_of_characters++;

        if (text_[index] == ' ' && line.size() >= 55 || index == text_.size() - 1) {
            push_line(line, num_of_characters);
        }
    }
}


ftxui::Element Text::get_text_element() {
    return ftxui::vbox(text_lines_) |
                ftxui::focusPosition(*position_x_, *position_y_) |
                ftxui::frame |
                ftxui::size(ftxui::HEIGHT, ftxui::EQUAL, 3);
}
