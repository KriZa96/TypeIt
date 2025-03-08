//
// Created by kil3 on 3/8/25.
//
#include <thread>
#include <gtest/gtest.h>

#define private public
#include "../include/Screen.h"
#undef private


TEST(ScreenTest, TestIsRunning) {
    Screen screen;
    ASSERT_TRUE(screen.refresh_);
}

TEST(ScreenTest, TestIsRunningAfterSomeTime) {
    Screen screen;
    std::this_thread::sleep_for(std::chrono::seconds(1));
    ASSERT_TRUE(screen.refresh_);
}

TEST(ScreenTest, TestIsStopped) {
    Screen screen;
    screen.stop_refresh();
    ASSERT_FALSE(screen.refresh_);
}

TEST(ScreenTest, TestIsStoppedAfterSomeTime) {
    Screen screen;
    std::this_thread::sleep_for(std::chrono::seconds(1));
    screen.stop_refresh();
    ASSERT_FALSE(screen.refresh_);
}

TEST(ScreenTest, TestIsRunningAndStoppedAfterSomeTime) {
    Screen screen;
    ASSERT_TRUE(screen.refresh_);

    std::this_thread::sleep_for(std::chrono::seconds(1));

    screen.stop_refresh();
    ASSERT_FALSE(screen.refresh_);
}