//
// Created by Kristijan Zalac on 3/8/25.
//
#include <thread>
#include <gtest/gtest.h>

#include "../include/core/Screen.h"


TEST(ScreenTest, TestIsRunning) {
    Screen screen;
    ASSERT_TRUE(screen.is_screen_refreshing());
}


TEST(ScreenTest, TestIsRunningAfterSomeTime) {
    Screen screen;
    std::this_thread::sleep_for(std::chrono::seconds(1));
    ASSERT_TRUE(screen.is_screen_refreshing());
}


TEST(ScreenTest, TestIsStopped) {
    Screen screen;
    screen.stop_refresh();
    ASSERT_FALSE(screen.is_screen_refreshing());
}


TEST(ScreenTest, TestIsStoppedAfterSomeTime) {
    Screen screen;
    std::this_thread::sleep_for(std::chrono::seconds(1));
    screen.stop_refresh();
    ASSERT_FALSE(screen.is_screen_refreshing());
}


TEST(ScreenTest, TestIsRunningAndStoppedAfterSomeTime) {
    Screen screen;
    ASSERT_TRUE(screen.is_screen_refreshing());

    std::this_thread::sleep_for(std::chrono::seconds(1));

    screen.stop_refresh();
    ASSERT_FALSE(screen.is_screen_refreshing());
}