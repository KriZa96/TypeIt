//
// Created by kil3 on 3/8/25.
//

#include "../../include/Text.h"
#include "../../include/FocusPosition.h"

#include <numeric>


Text::Text(std::string text):
    text_(text) {
    populate_text_lines();
}


void Text::push_line(ftxui::Elements& line, int& num_of_characters){
    text_lines_.push_back(ftxui::hbox(line) | ftxui::color(ftxui::Color::Grey62));  // TODO mozda ce trebat color stavit na svaki leement
    text_lines_size_.push_back(num_of_characters);
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


ftxui::Element Text::get_text_element() const {
    return ftxui::vbox(text_lines_) |
                ftxui::focusPosition(FocusPosition::x, FocusPosition::y) |
                ftxui::frame |
                ftxui::size(ftxui::HEIGHT, ftxui::EQUAL, 3);
}


ftxui::Component Text::get_text_component() {
    return ftxui::Renderer(
        [&] {
            return get_text_element();
        }
    );
}


int Text::get_text_size() const {
    return text_.size();
}


int Text::get_text_lines_size() const {
    return text_lines_.size();
}


std::string Text::get_text() const {
    return text_;
}


ftxui::Elements Text::get_text_lines() const {
    return text_lines_;
}


int Text::get_text_line_size(const int index) const {
    if (index < 0 || index >= text_lines_size_.size()) {
        return 0;
    }
    return text_lines_size_[index];
}

int Text::get_text_lines_size_up_to_index(const int index) const {
    if (index == 0) {
        return 0;
    }
    // TODO ovo moye moyda i bey gornjeg uvjeta
    return std::accumulate(
        text_lines_size_.begin(),
        text_lines_size_.begin() + index,
        0
    );
}
