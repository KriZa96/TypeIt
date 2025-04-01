//
// Created by kil3 on 3/9/25.
//

#include <utility>

#include "../../include/view/PerformanceArea.h"
#include "../../include/data/Style.h"


PerformanceArea::PerformanceArea(
    ftxui::Component timer_component,
    ftxui::Component accuracy_component,
    ftxui::Component word_calculator_component
):
    timer_component_(std::move(timer_component)),
    accuracy_component_(std::move(accuracy_component)),
    word_calculator_component_(std::move(word_calculator_component))
{}


ftxui::Component PerformanceArea::get_performance_component() const {
    return ftxui::Renderer(
        [&] {
            return ftxui::dbox(ftxui::flexbox({ftxui::dbox(ftxui::flexbox({
                accuracy_component_->Render(),
                timer_component_->Render(),
                word_calculator_component_->Render()},
                Style::space_between_config_)) |
                Style::performance_component_inside},
                Style::space_around_config_)) |
                Style::performance_component_outside;
        }
    );
}