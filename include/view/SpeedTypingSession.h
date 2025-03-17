//
// Created by kil3 on 3/9/25.
//

#ifndef SPEEDTYPINGSESSION_H
#define SPEEDTYPINGSESSION_H

#include "../core/Input.h"
#include "../core/Text.h"
#include "../core/WordCalculator.h"
#include "../interface/ITextSource.h"
#include "PerformanceArea.h"
#include "TextInputArea.h"
#include "ftxui/component/component.hpp"


class SpeedTypingSession {
    public:
        SpeedTypingSession();
        [[nodiscard]] ftxui::Component get_speed_typing_session_component() const;
    private:
        std::shared_ptr<ITextSource> text_source_;
        std::shared_ptr<Text> text_ptr_;
        std::shared_ptr<Input> input_ptr_;
        std::shared_ptr<Timer> timer_ptr_;
        std::shared_ptr<WordCalculator> word_calculator_ptr_;

        TextInputArea text_input_area_;
        PerformanceArea performance_area_;

        ftxui::Component text_input_area_component_;
        ftxui::Component performance_area_component_;
        ftxui::Component container_;
};



#endif //SPEEDTYPINGSESSION_H
