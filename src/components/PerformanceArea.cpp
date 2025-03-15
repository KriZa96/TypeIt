//
// Created by kil3 on 3/9/25.
//

#include <utility>

#include "../../include/PerformanceArea.h"
#include "../../include/GameOptions.h"


PerformanceArea::PerformanceArea(ftxui::Component timer_component, ftxui::Component word_calculator_component) :
    timer_component_(std::move(timer_component)),
    word_calculator_component_(std::move(word_calculator_component)),
    refresh_button_(ftxui::Button("Refresh", [this] {GameOptions::refresh_session_ = not GameOptions::refresh_session_;}, ftxui::ButtonOption::Ascii())),
    main_menu_button_(ftxui::Button("Menu", [this] {GameOptions::game_session_in_progress_ = not GameOptions::game_session_in_progress_;}, ftxui::ButtonOption::Ascii())),
    buttons_container_(ftxui::Container::Horizontal({refresh_button_, main_menu_button_}))
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
        buttons_container_,
        [&] {
            return ftxui::dbox(ftxui::flexbox({ftxui::dbox(ftxui::flexbox({
                timer_component_->Render(),
                buttons_container_->Render(),
                word_calculator_component_->Render()}, main_config_)) |
                size(ftxui::HEIGHT, ftxui::EQUAL, 3) |
                size(ftxui::WIDTH, ftxui::EQUAL, 65)}, secondary_config_)) |
                ftxui::border |
                size(ftxui::HEIGHT, ftxui::EQUAL, 3) |
                size(ftxui::WIDTH, ftxui::EQUAL, 75);
        }
    );
}