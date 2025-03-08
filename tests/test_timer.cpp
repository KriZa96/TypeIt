//
// Created by kil3 on 3/1/25.
//

#include <thread>
#include <gtest/gtest.h>

#define private public
#include "../include/Timer.h"
#undef private

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

TEST(TimerTest, RemainingTimeImmediate) {
    Timer timer(10);
    EXPECT_EQ(timer.get_time_left_str(), "10s");
}

TEST(TimerTest, RemainingTimeStringAfter1Sec) {
    Timer timer(10);
    std::this_thread::sleep_for(std::chrono::seconds(1));
    EXPECT_EQ(timer.get_time_left_str(), "9s");
}

TEST(TimerTest, RemainingTimeStringAfterMaxTime) {
    Timer timer(1);
    std::this_thread::sleep_for(std::chrono::seconds(2));
    EXPECT_EQ(timer.get_time_left_str(), "0s");
}
