//
// Created by kil3 on 3/9/25.
//

#ifndef PERFORMANCEAREA_H
#define PERFORMANCEAREA_H

#include "ftxui/component/component.hpp"

#include "Timer.h"
#include "WordCalculator.h"


class PerformanceArea {
    public:
        PerformanceArea(ftxui::Component timer_component, ftxui::Component word_calculator_component);
        ftxui::Component get_performance_component() const;
    private:
        ftxui::Component timer_component_;
        ftxui::Component word_calculator_component_;
        ftxui::FlexboxConfig config_;
};


#endif //PERFORMANCEAREA_H
