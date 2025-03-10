//
// Created by kil3 on 3/9/25.
//

#include "../../include/PerformanceArea.h"


PerformanceArea::PerformanceArea(int total_time, std::shared_ptr<int> word_count) :
    timer_(total_time),
    word_calculator_(timer_.get_elapsed_time_shared_pointer(), word_count) {
        config_.align_content = ftxui::FlexboxConfig::AlignContent::Center;
        config_.align_items = ftxui::FlexboxConfig::AlignItems::Center;
        config_.justify_content = ftxui::FlexboxConfig::JustifyContent::SpaceBetween;
    }


ftxui::Component PerformanceArea::get_performance_component() {
    return ftxui::Renderer(
        [&] {
            return ftxui::dbox(ftxui::flexbox({
                timer_.get_time_element(),
                word_calculator_.get_word_per_minute_element()}, config_)) |
                    ftxui::border |
                    size(ftxui::HEIGHT, ftxui::EQUAL, 3) |
                    size(ftxui::WIDTH, ftxui::EQUAL, 75);
        }
    );
}