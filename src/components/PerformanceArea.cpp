//
// Created by kil3 on 3/9/25.
//

#include <utility>

#include "../../include/PerformanceArea.h"


PerformanceArea::PerformanceArea(ftxui::Component timer_component, ftxui::Component word_calculator_component) :
    timer_component_(std::move(timer_component)),
    word_calculator_component_(std::move(word_calculator_component))
{
    config_.align_content = ftxui::FlexboxConfig::AlignContent::Center;
    config_.align_items = ftxui::FlexboxConfig::AlignItems::Center;
    config_.justify_content = ftxui::FlexboxConfig::JustifyContent::SpaceBetween;
}


ftxui::Component PerformanceArea::get_performance_component() const {
    return ftxui::Renderer(
        [&] {
            return ftxui::dbox(ftxui::flexbox({
                timer_component_->Render(),
                word_calculator_component_->Render()}, config_)) |
                    ftxui::border |
                    size(ftxui::HEIGHT, ftxui::EQUAL, 3) |
                    size(ftxui::WIDTH, ftxui::EQUAL, 75);
        }
    );
}