//
// Created by kil3 on 3/8/25.
//

#include "../../include/core/Input.h"

#include <iterator>
#include <numeric>
#include <utility>

#include "../../include/data/ComponentOptions.h"
#include "../../include/data/FocusPosition.h"
#include "../../include/data/GameState.h"
#include "../../include/data/Style.h"


Input::Input(std::shared_ptr<Text> text_instance) :
    current_line_index_(0),
    input_component_(ftxui::Input(&input_text_)),
    text_instance_(std::move(text_instance)),
    total_input_lines_(text_instance_->get_text_lines_size()+1, ftxui::hbox(ftxui::text("")))
    {}


const int& Input::get_word_count_reference() const {
    return input_word_count_.get_word_count_reference();
}


ftxui::Component Input::get_input_component() {
    return ftxui::Renderer(
        input_component_,
        [&] {
            render_input_text();
            input_word_count_.set_word_count(input_text_);
            total_input_lines_[current_line_index_] = ftxui::hbox(current_input_line_);
            return ftxui::vbox(total_input_lines_) |
                ftxui::focusPosition(FocusPosition::x, FocusPosition::y) |
                ftxui::frame |
                Style::text_input_element_style;
        }
    ) | ComponentOptions::input_component_decorator;
}


ftxui::Element Input::get_next_character() {
    ftxui::Element new_character = ftxui::text(std::string(1, input_text_.back()));
    if (input_text_.back() == text_instance_->get_char_at_line_and_position(
        current_line_index_, std::max(static_cast<int>(current_input_line_.size()), 0)
    )) {
        input_accuracy_.push_character_accuracy(true);
        return new_character | Style::input_text_color_good;
    }
    if (input_text_.back() == ' ') {
        new_character = ftxui::text(std::string(1, '_'));
    }
    input_accuracy_.push_character_accuracy(false);
    return new_character | Style::input_text_color_bad;
}


void Input::go_to_new_line() {
    total_input_lines_[current_line_index_++] = ftxui::hbox(current_input_line_);
    previous_input_lines_.push_back(current_input_line_);
    current_input_line_.clear();
    if (current_line_index_ < text_instance_->get_text_lines_size() - 1) {
        FocusPosition::y++;
    }
}


bool Input::should_go_to_next_line() const {
    return input_text_.back() == ' ' && current_line_index_ < text_instance_->get_text_lines_size() - 1 &&
        current_input_line_.size() >= text_instance_->get_text_line_size(current_line_index_);
}


void Input::add_element() {
    if (
        current_line_index_ + 1 >= text_instance_->get_text_lines_size() &&
        current_input_line_.size() + 1 >= text_instance_->get_text_line_size(current_line_index_)
        ) {
        GameState::game_finished = true;
        return;
    }
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
    if (current_line_index_ < text_instance_->get_text_lines_size() - 2) {
        FocusPosition::y--;
    }
}


bool Input::should_go_to_previous_line() const {
    return !input_text_.empty() && input_text_.size() < get_previous_lines_size();
}


void Input::remove_element() {
    if (should_go_to_previous_line()) {
        go_to_previous_line();
        return;
    }
    current_input_line_.pop_back();
}


bool Input::should_add_element() const {
    const size_t previous_elements_size = get_previous_lines_size() + current_input_line_.size();
    return input_text_.size() > previous_elements_size;
}


bool Input::should_remove_element() const {
    const size_t previous_elements_size = get_previous_lines_size() + current_input_line_.size();
    return input_text_.size() < previous_elements_size;
}


void Input::render_input_text() {
    if (should_add_element()) {
        add_element();
    }
    else if (should_remove_element()) {
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


ftxui::Component Input::get_accuracy_component() const {
    return ftxui::Renderer(
        [this] {
            return ftxui::text(std::format("acc: {:.1f}%", input_accuracy_.get_percentage_of_correct_input()));
        }
    );
}


