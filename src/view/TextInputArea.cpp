//
// Created by kil3 on 3/9/25.
//

#include <utility>

#include "../../include/view/TextInputArea.h"
#include "../../include/data/GameState.h"
#include "../../include/data/Style.h"


TextInputArea::TextInputArea(ftxui::Component input_component, ftxui::Component text_component):
    input_component_(std::move(input_component)),
    text_component_(std::move(text_component)),
    session_end_component_(ftxui::Renderer({
        [] {
            return ftxui::text("Session Finished!!!");
        }
    })),
    main_component_(ftxui::Container::Stacked(
    {
        ftxui::Maybe(ftxui::Container::Stacked( {
            text_component_,
            input_component_
        }), [&] {return !GameState::game_finished;}),
        ftxui::Maybe(session_end_component_, [&] {return GameState::game_finished;})
    }
    ))
{}

ftxui::Component TextInputArea::get_text_input_component() const {
    return ftxui::Renderer(
        input_component_,
        [&] {
            ftxui::Element main_element = ftxui::dbox(text_component_->Render(), input_component_->Render());
            if (GameState::game_finished) {
                main_element = ftxui::text("Session Finished!!!");
            }
            return ftxui::flexbox({
                main_element |
                ftxui::center |
                ftxui::border |
                size(ftxui::HEIGHT, ftxui::EQUAL, 10) |
                size(ftxui::WIDTH, ftxui::EQUAL, 75)
            }, Style::full_center_config_);
        }
    );
}

