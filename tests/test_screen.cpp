//
// Created by kil3 on 3/8/25.
//
#include <thread>
#include <gtest/gtest.h>

#define private public
#include "../src/components/Screen.h"
#undef private

TEST(ScreenTest, TestIsRunning) {
    Screen screen;
    ASSERT_TRUE(screen.running_);
}

TEST(ScreenTest, TestIsRunningAfterSomeTime) {
    Screen screen;
    std::this_thread::sleep_for(std::chrono::seconds(1));
    ASSERT_TRUE(screen.running_);
}

TEST(ScreenTest, TestIsStopped) {
    Screen screen;
    screen.stop_refresh();
    ASSERT_FALSE(screen.running_);
}

TEST(ScreenTest, TestIsStoppedAfterSomeTime) {
    Screen screen;
    std::this_thread::sleep_for(std::chrono::seconds(1));
    screen.stop_refresh();
    ASSERT_FALSE(screen.running_);
}

TEST(ScreenTest, TestIsRunningAndStoppedAfterSomeTime) {
    Screen screen;
    ASSERT_TRUE(screen.running_);

    std::this_thread::sleep_for(std::chrono::seconds(1));

    screen.stop_refresh();
    ASSERT_FALSE(screen.running_);
}