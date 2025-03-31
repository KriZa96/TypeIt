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
        int get_elapsed_time();
        ftxui::Element get_time_element();
        ftxui::Component get_time_component();
        int& get_elapsed_time_reference();
    private:
        std::chrono::steady_clock::time_point start_time_;
        int total_time_;
        int elapsed_time_;
        bool started_timer_;

        std::string get_time_left_str();

        FRIEND_TEST(TimerTest, ElapsedTimeStartsAtZero);
        FRIEND_TEST(TimerTest, ElapsedTimeAfterDelay);
        FRIEND_TEST(TimerTest, ElapsedTimeAfterMaxTime);
        FRIEND_TEST(TimerTest, RemainingTimeImmediate);
        FRIEND_TEST(TimerTest, RemainingTimeStringAfter1Sec);
        FRIEND_TEST(TimerTest, RemainingTimeStringAfterMaxTime);
        FRIEND_TEST(TimerTest, DosentCalculateWhenStartGameFalse);
};


#endif //TIMER_H
