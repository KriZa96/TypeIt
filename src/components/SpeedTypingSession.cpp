//
// Created by kil3 on 3/9/25.
//

#include "../../include/SpeedTypingSession.h"


SpeedTypingSession::SpeedTypingSession(int total_time, const std::string& text):
    text_ptr_(std::make_shared<Text>(text)),
    input_ptr_(std::make_shared<Input>(text_ptr_)),
    timer_ptr_(std::make_shared<Timer>(total_time)),
    word_calculator_ptr_(
        std::make_shared<WordCalculator>(
            timer_ptr_->get_elapsed_time_reference(),
            input_ptr_->get_word_count_reference())
    ),

    text_input_area_(input_ptr_->get_input_component(), text_ptr_->get_text_component()),
    performance_area_(timer_ptr_->get_time_component(), word_calculator_ptr_->get_word_calculator_component()),

    text_input_area_component_(text_input_area_.get_text_input_component()),
    performance_area_component_(performance_area_.get_performance_component())
{
    config_.align_content = ftxui::FlexboxConfig::AlignContent::Center;
    config_.align_items = ftxui::FlexboxConfig::AlignItems::Center;
    config_.justify_content = ftxui::FlexboxConfig::JustifyContent::Center;
}


ftxui::Component SpeedTypingSession::get_speed_typing_session_component() const {
    return ftxui::Renderer(
        text_input_area_component_,
        [&] {
            return ftxui::flexbox({ftxui::vbox(performance_area_component_->Render(), text_input_area_component_->Render())}, config_) | ftxui::border;
        }
    );
}