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
        PerformanceArea(int total_time, std::shared_ptr<int> word_count);
        ftxui::Component get_performance_component();
    private:
        Timer timer_;
        WordCalculator word_calculator_;
        ftxui::FlexboxConfig config_;
};


#endif //PERFORMANCEAREA_H
