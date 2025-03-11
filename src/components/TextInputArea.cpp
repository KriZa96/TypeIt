//
// Created by kil3 on 3/9/25.
//

#include <utility>

#include "../../include/TextInputArea.h"


TextInputArea::TextInputArea(ftxui::Component input_component, ftxui::Component text_component):
    input_component_(std::move(input_component)),
    text_component_(std::move(text_component))
{
    config_.align_content = ftxui::FlexboxConfig::AlignContent::Center;
    config_.align_items = ftxui::FlexboxConfig::AlignItems::Center;
    config_.justify_content = ftxui::FlexboxConfig::JustifyContent::Center;
}


ftxui::Component TextInputArea::get_text_input_component() const {
    return ftxui::Renderer(
        input_component_,
        [&] {
            return ftxui::flexbox({
                ftxui::dbox(text_component_->Render(), input_component_->Render()) |
                ftxui::center |
                ftxui::border |
                size(ftxui::HEIGHT, ftxui::EQUAL, 10) |
                size(ftxui::WIDTH, ftxui::EQUAL, 75)
            }, config_);
        }
    );
}

