//
// Created by Kristijan Zalac on 3/8/25.
//

#include "../../include/core/Input.h"
#include "../../include/data/ComponentOptions.h"


Input::Input(const Text& text_instance) :
    input_line_(text_instance),
    input_component_(ftxui::Input(&input_text_))
{}


const int& Input::get_word_count_reference() const {
    return input_word_count_.get_word_count_reference();
}


ftxui::Component Input::get_input_component() {
    return ftxui::Renderer(
        input_component_,
        [&] {
            input_line_.render_input_text(input_text_.empty() ? ' ' : input_text_.back(), input_text_.size());
            input_word_count_.set_word_count(input_text_);
            return ftxui::vbox(input_line_.get_total_input_lines()) |
                ftxui::focusPosition(FocusPosition::x, FocusPosition::y) |
                ftxui::frame |
                Style::text_input_element_style;
        }
    ) | ComponentOptions::input_component_decorator;
}


ftxui::Component Input::get_accuracy_component() const {
    return ftxui::Renderer(
        [this] {
            return ftxui::text(std::format("acc: {:.1f}%", input_line_.get_percentage_of_correct_input()));
        }
    );
}
