//
// Created by kil3 on 3/9/25.
//

#ifndef SPEEDTYPINGSESSION_H
#define SPEEDTYPINGSESSION_H

#include "ftxui/component/component.hpp"

#include "PerformanceArea.h"
#include "TextInputArea.h"


class SpeedTypingSession {
    public:
        SpeedTypingSession(int total_time, std::string text);
        ftxui::Component get_speed_typing_session_component();
        void restart();
    private:
        TextInputArea text_input_area_;
        PerformanceArea performance_area_;
        ftxui::Component text_input_area_component_;
        ftxui::Component performance_area_component_;
        ftxui::FlexboxConfig config_;
};



#endif //SPEEDTYPINGSESSION_H
