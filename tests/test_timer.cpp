//
// Created by kil3 on 3/1/25.
//
#include <chrono>
#include <thread>
#include <gtest/gtest.h>
#include <ftxui/dom/elements.hpp>

#include "../src/components/Timer.h"


TEST(TimerTest, ElapsedTimeStartsAtZero) {
    Timer timer(10);
    EXPECT_FLOAT_EQ(timer.get_elapsed_time(), 0);
}

TEST(TimerTest, ElapsedTimeAfterDelay) {
    Timer timer(10);
    std::this_thread::sleep_for(std::chrono::seconds(1));
    EXPECT_GE(timer.get_elapsed_time(), 1);
}

TEST(TimerTest, ElapsedTimeAfterMaxTime) {
    Timer timer(1);
    std::this_thread::sleep_for(std::chrono::seconds(2));
    EXPECT_GE(timer.get_elapsed_time(), 1);
}
