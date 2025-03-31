//
// Created by kil3 on 3/1/25.
//

#ifndef TIMER_H
#define TIMER_H

#include <chrono>

#include "gtest/gtest_prod.h"
#include "ftxui/component/component.hpp"


class Timer {
    public:
        explicit Timer(int total_time);
        void start_timer();
        int get_elapsed_time();
        std::string get_time_left_str();
        int& get_elapsed_time_reference();
        ftxui::Element get_time_element();
        ftxui::Component get_time_component();
    private:
        std::chrono::steady_clock::time_point start_time_;
        int total_time_;
        int elapsed_time_;
        bool started_timer_;
};


#endif //TIMER_H
