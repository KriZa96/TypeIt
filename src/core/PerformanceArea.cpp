//
// Created by kil3 on 3/9/25.
//

#include "../../include/core/PerformanceArea.h"

#include <utility>


PerformanceArea::PerformanceArea(ftxui::Component timer_component, ftxui::Component word_calculator_component) :
    timer_component_(std::move(timer_component)),
    word_calculator_component_(std::move(word_calculator_component))
{
    main_config_.align_content = ftxui::FlexboxConfig::AlignContent::Center;
    main_config_.align_items = ftxui::FlexboxConfig::AlignItems::Center;
    main_config_.justify_content = ftxui::FlexboxConfig::JustifyContent::SpaceBetween;

    secondary_config_.align_content = ftxui::FlexboxConfig::AlignContent::Center;
    secondary_config_.align_items = ftxui::FlexboxConfig::AlignItems::Center;
    secondary_config_.justify_content = ftxui::FlexboxConfig::JustifyContent::SpaceAround;
}


ftxui::Component PerformanceArea::get_performance_component() const {
    return ftxui::Renderer(
        [&] {
            return ftxui::dbox(ftxui::flexbox({ftxui::dbox(ftxui::flexbox({
                timer_component_->Render(),
                ftxui::text("Refresh [Ctrl+r]"),
                ftxui::text("Menu [Ctrl+t]"),
                word_calculator_component_->Render()}, main_config_)) |
                size(ftxui::HEIGHT, ftxui::EQUAL, 3) |
                size(ftxui::WIDTH, ftxui::EQUAL, 65)}, secondary_config_)) |
                ftxui::border |
                size(ftxui::HEIGHT, ftxui::EQUAL, 3) |
                size(ftxui::WIDTH, ftxui::EQUAL, 75);
        }
    );
}