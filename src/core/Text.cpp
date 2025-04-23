//
// Created by kil3 on 3/8/25.
//


#include <utility>

#include "../../include/data/Style.h"
#include "../../include/core/Text.h"


Text::Text(std::string text):
    text_(std::move(text)) {
    populate_text_lines();
}


void Text::push_line(ftxui::Elements& line, std::string& text, int& num_of_characters){
    text_lines_.push_back(ftxui::hbox(line) | Style::underlying_text_color);
    text_lines_strings_.push_back(text);
    text_lines_size_.push_back(num_of_characters);
    num_of_characters = 0;
    line.clear();
    text.clear();
}


void Text::populate_text_lines() {
    int num_of_characters = 0;
    ftxui::Elements line;
    std::string text;

    for (int index = 0; index < text_.size(); index++) {
        char current_character = text_[index];

        // Newline characters are treated as spaces to simulate uninterrupted flow.
        // This ensures users type as if the text is a single continuous line
        line.push_back(ftxui::text(std::string(1, current_character == '\n' ? ' ' : current_character)));
        text += current_character == '\n' ? ' ' : current_character;
        num_of_characters++;

        // Determine whether to start a new visual line:
        // - If the current character is a newline
        // - If the line exceeds 55 characters and ends at a space
        // - If this is the last character in the input
        bool should_go_to_next_line = (
            current_character == '\n' || (current_character == ' ' && line.size() >= 55 || index == text_.size() - 1)
        );
        if (should_go_to_next_line) {
            push_line(line, text, num_of_characters);
        }
    }
}


ftxui::Element Text::get_text_element() const {
    return ftxui::vbox(text_lines_) |
        ftxui::focusPosition(FocusPosition::x, FocusPosition::y) |
        ftxui::frame |
        Style::text_input_element_style;
}


ftxui::Component Text::get_text_component() const {
    return ftxui::Renderer(
        [&] {
            return get_text_element();
        }
    );
}


int Text::get_text_lines_size() const {
    return static_cast<int>(text_lines_.size());
}


int Text::get_text_line_size(const int index) const {
    if (index < 0 || index >= text_lines_size_.size()) {
        return 0;
    }
    return text_lines_size_[index];
}


std::string Text::get_text() const {
    return text_;
}


char Text::get_char_at_line_and_position(const int line_index, const int char_index) const {
    bool character_out_of_bounds = (
        line_index < 0 || line_index >= text_lines_strings_.size() ||
        char_index < 0 || char_index >= text_lines_size_[line_index]
    );

    if (character_out_of_bounds) {
        return '\0';
    }
    return text_lines_strings_[line_index][char_index];
}


ftxui::Elements Text::get_text_lines() const {
    return text_lines_;
}
