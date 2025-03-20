//
// Created by kil3 on 3/9/25.
//

#include "../../include/view/SpeedTypingSession.h"
#include "../../include/core/FileTextSource.h"
#include "../../include/data/GameOptions.h"
#include "../../include/data/Style.h"


SpeedTypingSession::SpeedTypingSession():
    text_source_(std::make_shared<FileTextSource>(GameOptions::text_radiobox_values_[GameOptions::selected_radiobox_text_])),
    text_ptr_(std::make_shared<Text>(text_source_->get_text())),
    input_ptr_(std::make_shared<Input>(text_ptr_)),
    timer_ptr_(std::make_shared<Timer>(GameOptions::time_radiobox_values_[GameOptions::selected_radiobox_time_])),
    word_calculator_ptr_(
        std::make_shared<WordCalculator>(
            timer_ptr_->get_elapsed_time_reference(),
            input_ptr_->get_word_count_reference())
    ),

    text_input_area_(input_ptr_->get_input_component(), text_ptr_->get_text_component()),
    performance_area_(timer_ptr_->get_time_component(), word_calculator_ptr_->get_word_calculator_component()),

    text_input_area_component_(text_input_area_.get_text_input_component()),
    performance_area_component_(performance_area_.get_performance_component())
{}


ftxui::Component SpeedTypingSession::get_speed_typing_session_component() const {
    return ftxui::Renderer(
        text_input_area_component_,
        [&] {
            return ftxui::flexbox(
                {
                    performance_area_component_->Render(),
                    text_input_area_component_->Render()
                },
                Style::full_center_config_
            ) | ftxui::border;
        }
    );
}