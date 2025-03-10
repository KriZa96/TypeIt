//
// Created by kil3 on 3/8/25.
//

#include "../../include/Input.h"
#include "../../include/FocusPosition.h"


Input::Input(
    std::shared_ptr<Text> text_instance
) :
word_count_(3),
current_line_index_(0),
input_component_(ftxui::Input(&input_text_)),
text_instance_(text_instance),
total_input_lines_(text_instance_->get_text_lines_size(), ftxui::hbox(ftxui::text("")))
{}


ftxui::Component Input::get_input_component() {
    return ftxui::Renderer(
        input_component_,
        [&] {
            render_input_text();
            total_input_lines_[current_line_index_] = ftxui::hbox(current_input_line_);
            return ftxui::vbox(total_input_lines_) |
                ftxui::focusPosition(FocusPosition::x, FocusPosition::y) |
                ftxui::frame |
                ftxui::size(ftxui::HEIGHT, ftxui::EQUAL, 3);
        }
    );
}


ftxui::Element Input::get_next_character(const int character_index) const {
    ftxui::Element new_character = ftxui::text(std::string(1, input_text_.back()));
    if (text_instance_->get_text_size() >= character_index && text_instance_->get_text()[character_index - 1] == input_text_.back()) {
        return new_character | ftxui::color(ftxui::Color::Grey82);
    }
    if (input_text_.back() == ' ') {
        new_character = ftxui::text(std::string(1, '_'));
    }
    return new_character | ftxui::color(ftxui::Color::Salmon1);
}


void Input::go_to_new_line() {
    //current_input_line_.push_back(get_next_character(input_text_.size()));
    total_input_lines_[current_line_index_++] = ftxui::hbox(current_input_line_);
    previous_input_lines_.push_back(current_input_line_);
    current_input_line_.clear();
    FocusPosition::y++;
}


void Input::go_to_previous_line() {
    total_input_lines_[current_line_index_] = ftxui::hbox();
    current_line_index_ ? current_line_index_-- : 0;
    current_input_line_ = previous_input_lines_[current_line_index_];
    previous_input_lines_.pop_back();
    FocusPosition::y--;
}

void Input::add_element() {
    current_input_line_.push_back(get_next_character(input_text_.size()));
    if (
        input_text_.back() == ' ' && current_input_line_.size() + 1 >=
        text_instance_->get_text_line_size(current_line_index_)
    ) {
        go_to_new_line();
    }
}


void Input::remove_element() {
    if (!input_text_.empty() && input_text_.size() <= get_previous_size()) {
        go_to_previous_line();
        return;
    }
    current_input_line_.pop_back();
}


void Input::render_input_text() {
    size_t previous_elements_size = get_previous_size() + current_input_line_.size();
    if (input_text_.size() > previous_elements_size) {
        input_text_.back() == ' ' ? word_count_++ : word_count_;
        add_element();
    }
    else if (input_text_.size() < previous_elements_size) {
        input_text_.back() == ' ' && word_count_ ? word_count_-- : word_count_;
        remove_element();
    }
}

std::shared_ptr<int> Input::get_word_count_shared_pointer() {
    return std::make_shared<int>(word_count_);
}


int Input::get_previous_size() {
    int sum = 0;
    for (const auto& line : previous_input_lines_) {
        for (auto car : line) {
            sum++;
        }
    }
    return sum;
}
