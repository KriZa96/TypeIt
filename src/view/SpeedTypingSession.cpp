//
// Created by Kristijan Zalac on 3/9/25.
//

#include "../../include/view/SpeedTypingSession.h"
#include "../../include/core/FileTextSource.h"
#include "../../include/data/GameOptions.h"
#include "../../include/data/Style.h"


SpeedTypingSession::SpeedTypingSession():
    text_source_(std::make_shared<FileTextSource>(GameOptions::text_radiobox_values_[GameOptions::selected_radiobox_text_])),
    text_(text_source_->get_text()),
    input_(text_),
    timer_(GameOptions::time_radiobox_values_[GameOptions::selected_radiobox_time_]),
    word_calculator_(
            timer_.get_elapsed_time_reference(),
            input_.get_word_count_reference()
    ),
    text_input_area_(input_.get_input_component(), text_.get_text_component()),
    performance_area_(
        timer_.get_time_component(),
        input_.get_accuracy_component(),
        word_calculator_.get_word_calculator_component()
    ),
    text_input_area_component_(text_input_area_.get_text_input_component()),
    performance_area_component_(performance_area_.get_performance_component())
{}


ftxui::Component SpeedTypingSession::get_speed_typing_session_component() const {
    return ftxui::Renderer(
        text_input_area_component_,
        [&] {
            return ftxui::flexbox(
                {
                    ftxui::vbox(
                        performance_area_component_->Render(),
                        text_input_area_component_->Render(),
                        ftxui::dbox(ftxui::flexbox({ftxui::dbox(ftxui::flexbox({
                            ftxui::text("Refresh [Ctrl+r]"),
                            ftxui::text("Menu [Ctrl+t]")},
                            Style::space_between_config_)) |
                            Style::game_session_actions},
                            Style::space_around_config_))
                    )
                },
                Style::full_center_config_
            );
        }
    );
}