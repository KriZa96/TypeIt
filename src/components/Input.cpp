//
// Created by kil3 on 3/8/25.
//

#include "../../include/Input.h"

#include <iterator>
#include <numeric>
#include <utility>

#include "../../include/FocusPosition.h"


Input::Input(std::shared_ptr<Text> text_instance) :
    word_count_(3),
    current_line_index_(0),
    input_component_(ftxui::Input(&input_text_)),
    text_instance_(std::move(text_instance)),
    total_input_lines_(text_instance_->get_text_lines_size(), ftxui::hbox(ftxui::text("")))
    {}


const int& Input::get_word_count_reference() const {
    return word_count_;
}


ftxui::Component Input::get_input_component() {
    return ftxui::Renderer(
        input_component_,
        [&] {
            render_input_text();
            set_amount_of_words();
            total_input_lines_[current_line_index_] = ftxui::hbox(current_input_line_);
            return ftxui::vbox(total_input_lines_) |
                ftxui::focusPosition(FocusPosition::x, FocusPosition::y) |
                ftxui::frame |
                ftxui::size(ftxui::HEIGHT, ftxui::EQUAL, 3);
        }
    );
}


ftxui::Element Input::get_next_character() const {
    ftxui::Element new_character = ftxui::text(std::string(1, input_text_.back()));
    if (input_text_.back() == text_instance_->get_char_at_line_and_position(
        current_line_index_, std::max(static_cast<int>(current_input_line_.size()), 0)
    )) {
        return new_character | ftxui::color(ftxui::Color::Grey82);
    }
    if (input_text_.back() == ' ') {
        new_character = ftxui::text(std::string(1, '_'));
    }
    return new_character | ftxui::color(ftxui::Color::Salmon1);
}


void Input::go_to_new_line() {
    total_input_lines_[current_line_index_++] = ftxui::hbox(current_input_line_);
    previous_input_lines_.push_back(current_input_line_);
    current_input_line_.clear();
    FocusPosition::y++;
}


bool Input::should_go_to_next_line() const {
    return input_text_.back() == ' ' && current_line_index_ < text_instance_->get_text_lines_size() &&
        current_input_line_.size() + 1 >= text_instance_->get_text_line_size(current_line_index_);
}


void Input::add_element() {
    current_input_line_.push_back(get_next_character());
    if (should_go_to_next_line()) {
        go_to_new_line();
    }
}


void Input::go_to_previous_line() {
    total_input_lines_[current_line_index_] = ftxui::hbox();
    current_line_index_ ? current_line_index_-- : 0;
    current_input_line_ = previous_input_lines_[current_line_index_];
    previous_input_lines_.pop_back();
    FocusPosition::y--;
}


bool Input::should_go_to_previous_line() const {
    return !input_text_.empty() && input_text_.size() <= get_previous_lines_size();
}


void Input::remove_element() {
    if (should_go_to_previous_line()) {
        go_to_previous_line();
        return;
    }
    current_input_line_.pop_back();
}


void Input::render_input_text() {
    size_t previous_elements_size = get_previous_lines_size() + current_input_line_.size();
    if (input_text_.size() > previous_elements_size) {
        add_element();
    }
    else if (input_text_.size() < previous_elements_size) {
        remove_element();
    }
}


int Input::get_previous_lines_size() const {
    return std::accumulate(
        previous_input_lines_.begin(),
        previous_input_lines_.end(),
        0,
        [](const int sum, const ftxui::Elements& line) {
            return sum + std::distance(line.begin(), line.end());
        });
}


void Input::set_amount_of_words() {
    std::istringstream stream(input_text_);
    word_count_ = static_cast<int>(
        std::distance(std::istream_iterator<std::string>(stream),
        std::istream_iterator<std::string>())
    );
}