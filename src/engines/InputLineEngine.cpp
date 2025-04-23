//
// Created by kriza on 01/04/2025.
//

#include <numeric>
#include <utility>

#include "../../include/engines/InputLineEngine.h"
#include "../../include/data/Style.h"
#include "../../include/data/GameState.h"


InputLineEngine::InputLineEngine(std::shared_ptr<Text> text_instance):
    current_line_index_(0),
    text_instance_(std::move(text_instance)),
    total_input_lines_(text_instance_->get_text_lines_size()+1, ftxui::hbox(ftxui::text("")))
{}


[[nodiscard]] int InputLineEngine::get_previous_lines_size() const {
    return std::accumulate(
        previous_input_lines_.begin(),
        previous_input_lines_.end(),
        0,
        [](const int sum, const ftxui::Elements& line) {
            return sum + std::distance(line.begin(), line.end());
        });
}


void InputLineEngine::go_to_new_line() {
    total_input_lines_[current_line_index_++] = ftxui::hbox(current_input_line_);
    previous_input_lines_.push_back(current_input_line_);
    current_input_line_.clear();
    if (current_line_index_ < text_instance_->get_text_lines_size() - 1) {
        FocusPosition::y++;
    }
}


bool InputLineEngine::should_go_to_next_line(char next_character) const {
    return next_character == ' ' && current_line_index_ < text_instance_->get_text_lines_size() - 1 &&
           current_input_line_.size() >= text_instance_->get_text_line_size(current_line_index_);
}


bool InputLineEngine::should_finish_game() {
    return current_line_index_ + 1 >= text_instance_->get_text_lines_size() &&
           current_input_line_.size() + 1 >= text_instance_->get_text_line_size(current_line_index_);
}


void InputLineEngine::add_element(char next_character) {
    if (should_finish_game()) {
        GameState::game_finished = true;
        return;
    }
    current_input_line_.push_back(get_next_character(next_character));
    if (should_go_to_next_line(next_character)) {
        go_to_new_line();
    }
}


void InputLineEngine::go_to_previous_line() {
    total_input_lines_[current_line_index_] = ftxui::hbox();
    current_line_index_ ? current_line_index_-- : 0;
    current_input_line_ = previous_input_lines_[current_line_index_];
    previous_input_lines_.pop_back();
    if (current_line_index_ < text_instance_->get_text_lines_size() - 2) {
        FocusPosition::y--;
    }
}


void InputLineEngine::remove_element(std::size_t input_text_size) {
    if (input_text_size < get_previous_lines_size()) {
        go_to_previous_line();
        return;
    }
    current_input_line_.pop_back();
}


bool InputLineEngine::should_add_element(std::size_t input_text_size) const {
    const size_t previous_elements_size = get_previous_lines_size() + current_input_line_.size();
    return input_text_size > previous_elements_size;
}


bool InputLineEngine::should_remove_element(std::size_t input_text_size) const {
    const size_t previous_elements_size = get_previous_lines_size() + current_input_line_.size();
    return input_text_size < previous_elements_size;
}


void InputLineEngine::render_input_text(char next_character, std::size_t input_text_size) {
    if (should_add_element(input_text_size)) {
        add_element(next_character);
    }
    else if (should_remove_element(input_text_size)) {
        remove_element(input_text_size);
    }
    // Update total input line with current line
    total_input_lines_[current_line_index_] = ftxui::hbox(current_input_line_);
}


// Transforms character to ftxui element.
[[nodiscard]] ftxui::Element InputLineEngine::get_next_character(const char next_character) {
    ftxui::Element new_character = ftxui::text(std::string(1, next_character));
    if (next_character == text_instance_->get_char_at_line_and_position(
  current_line_index_, std::max(static_cast<int>(current_input_line_.size()), 0)
          )) {
        input_accuracy_.push_character_accuracy(true);
        return new_character | Style::input_text_color_good;
    }
    // If the character is a space, replace it with an underscore for visual purposes.
    if (next_character == ' ') {
        new_character = ftxui::text(std::string(1, '_'));
    }
    input_accuracy_.push_character_accuracy(false);
    return new_character | Style::input_text_color_bad;
}


float InputLineEngine::get_percentage_of_correct_input() const {
    return input_accuracy_.get_percentage_of_correct_input();
}


ftxui::Elements InputLineEngine::get_total_input_lines() const {
    return total_input_lines_;
}


std::size_t InputLineEngine::get_current_line_index() const {
    return current_line_index_;
}


[[nodiscard]] std::size_t InputLineEngine::get_current_line_size() const {
    return current_input_line_.size();
}
