//
// Created by kil3 on 3/9/25.
//

#include "../../include/SpeedTypingSession.h"


SpeedTypingSession::SpeedTypingSession(int total_time, std::string text):
    text_input_area_(text),
    performance_area_(total_time, text_input_area_.get_word_count_shared_pointer()),
    text_input_area_component_(text_input_area_.get_text_input_component()),
    performance_area_component_(performance_area_.get_performance_component()){
    config_.align_content = ftxui::FlexboxConfig::AlignContent::Center;
    config_.align_items = ftxui::FlexboxConfig::AlignItems::Center;
    config_.justify_content = ftxui::FlexboxConfig::JustifyContent::Center;
}


ftxui::Component SpeedTypingSession::get_speed_typing_session_component() {
    return ftxui::Renderer(
        text_input_area_component_,
        [&] {
            return ftxui::flexbox({performance_area_component_->Render(), text_input_area_component_->Render()}, config_) | ftxui::border;
        }
    );
}