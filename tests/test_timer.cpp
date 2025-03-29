//
// Created by kil3 on 3/1/25.
//

#include <thread>
#include <gtest/gtest.h>

#define private public
#include "../include/core/Timer.h"
#undef private
#include "../include/data/GameState.h"


TEST(TimerTest, ElapsedTimeStartsAtZero) {
    Timer timer(10);
    GameState::game_session_in_progress = true;
    timer.started_timer_ = true;
    GameState::game_finished = false;
    EXPECT_FLOAT_EQ(timer.get_elapsed_time(), 0);
}


TEST(TimerTest, ElapsedTimeAfterDelay) {
    Timer timer(10);
    GameState::game_session_in_progress = true;
    timer.started_timer_ = true;
    GameState::game_finished = false;
    std::this_thread::sleep_for(std::chrono::seconds(1));
    EXPECT_GE(timer.get_elapsed_time(), 1);
}


TEST(TimerTest, ElapsedTimeAfterMaxTime) {
    Timer timer(1);
    GameState::game_session_in_progress = true;
    timer.started_timer_ = true;
    GameState::game_finished = false;
    std::this_thread::sleep_for(std::chrono::seconds(2));
    EXPECT_GE(timer.get_elapsed_time(), 1);
}


TEST(TimerTest, RemainingTimeImmediate) {
    Timer timer(10);
    GameState::game_session_in_progress = true;
    GameState::game_finished = false;
    timer.started_timer_ = true;
    EXPECT_EQ(timer.get_time_left_str(), "10s");
}


TEST(TimerTest, RemainingTimeStringAfter1Sec) {
    Timer timer(10);
    GameState::game_session_in_progress = true;
    GameState::game_finished = false;
    timer.started_timer_ = true;
    std::this_thread::sleep_for(std::chrono::seconds(1));
    EXPECT_EQ(timer.get_time_left_str(), "9s");
}


TEST(TimerTest, RemainingTimeStringAfterMaxTime) {
    Timer timer(1);
    GameState::game_session_in_progress = true;
    GameState::game_finished = false;
    timer.started_timer_ = true;
    std::this_thread::sleep_for(std::chrono::seconds(2));
    EXPECT_EQ(timer.get_time_left_str(), "0s");
}

TEST(TimerTest, DosentCalculateWhenStartGameFalse) {
    Timer timer(10);
    GameState::game_finished = false;
    std::this_thread::sleep_for(std::chrono::seconds(2));
    EXPECT_EQ(timer.get_time_left_str(), "10s");
}