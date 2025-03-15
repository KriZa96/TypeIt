//
// Created by kil3 on 3/9/25.
//

#ifndef PERFORMANCEAREA_H
#define PERFORMANCEAREA_H

#include "ftxui/component/component.hpp"
#include "Timer.h"


class PerformanceArea {
    public:
        PerformanceArea(ftxui::Component timer_component, ftxui::Component word_calculator_component);
        [[nodiscard]] ftxui::Component get_performance_component() const;
    private:
        ftxui::Component timer_component_;
        ftxui::Component word_calculator_component_;
        ftxui::FlexboxConfig main_config_;
        ftxui::FlexboxConfig secondary_config_;
};


#endif //PERFORMANCEAREA_H
