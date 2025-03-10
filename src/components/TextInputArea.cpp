//
// Created by kil3 on 3/9/25.
//

#include "../../include/TextInputArea.h"


TextInputArea::TextInputArea(std::string text):
    text_(text),
    input_(std::make_shared<Text>(text_)),
    input_component_(input_.get_input_component()) {
        config_.align_content = ftxui::FlexboxConfig::AlignContent::Center;
        config_.align_items = ftxui::FlexboxConfig::AlignItems::Center;
        config_.justify_content = ftxui::FlexboxConfig::JustifyContent::Center;
    }


ftxui::Component TextInputArea::get_text_input_component() {
    // TODO tu staviti input_component_ = input_.get...
    return ftxui::Renderer(
        input_component_,
        [&] {
            return ftxui::flexbox({
                ftxui::dbox(text_.get_text_element(), input_component_->Render()) |
                ftxui::center |
                ftxui::border |
                size(ftxui::HEIGHT, ftxui::EQUAL, 10) |
                size(ftxui::WIDTH, ftxui::EQUAL, 75)
            }, config_);
        }
    );
}


std::shared_ptr<int> TextInputArea::get_word_count_shared_pointer() {
    return input_.get_word_count_shared_pointer();
}
