//
// Created by Kristijan Zalac on 3/9/25.
//

#ifndef PERFORMANCEAREA_H
#define PERFORMANCEAREA_H

#include "ftxui/component/component.hpp"
#include "../core/Timer.h"


class PerformanceArea {
    public:
        PerformanceArea(
            ftxui::Component timer_component,
            ftxui::Component accuracy_component,
            ftxui::Component word_calculator_component
        );
        [[nodiscard]] ftxui::Component get_performance_component() const;
    private:
        ftxui::Component timer_component_;
        ftxui::Component accuracy_component_;
        ftxui::Component word_calculator_component_;
};


#endif //PERFORMANCEAREA_H
