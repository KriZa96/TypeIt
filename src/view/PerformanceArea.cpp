//
// Created by kil3 on 3/9/25.
//

#include <utility>

#include "../../include/view/PerformanceArea.h"
#include "../../include/data/Style.h"



PerformanceArea::PerformanceArea(ftxui::Component timer_component, ftxui::Component word_calculator_component) :
    timer_component_(std::move(timer_component)),
    word_calculator_component_(std::move(word_calculator_component))
{}


ftxui::Component PerformanceArea::get_performance_component() const {
    return ftxui::Renderer(
        [&] {
            return ftxui::dbox(ftxui::flexbox({ftxui::dbox(ftxui::flexbox({
                timer_component_->Render(),
                ftxui::text("Refresh [Ctrl+r]"),
                ftxui::text("Menu [Ctrl+t]"),
                word_calculator_component_->Render()}, Style::space_between_config_)) |
                size(ftxui::HEIGHT, ftxui::EQUAL, 3) |
                size(ftxui::WIDTH, ftxui::EQUAL, 65)}, Style::space_around_config_)) |
                ftxui::border |
                size(ftxui::HEIGHT, ftxui::EQUAL, 3) |
                size(ftxui::WIDTH, ftxui::EQUAL, 75);
        }
    );
}