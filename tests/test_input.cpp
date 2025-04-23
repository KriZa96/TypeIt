//
// Created by kil3 on 3/9/25.
//

#include <gtest/gtest.h>
#include "../include/core/Text.h"
#include "../include/data/GameState.h"
#include "../include/data/FocusPosition.h"
#include "../include/core/Input.h"


class InputTest : public ::testing::Test {
protected:
    InputTest()
        : text("line1\nline2"),
        input(text) {}

    void SetUp() override {
        GameState::game_session_in_progress = true;
        GameState::game_finished = false;
        GameState::refresh_session = false;
        FocusPosition::x = 0;
        FocusPosition::y = 0;
    }

    Text text;
    Input input;
};


TEST_F(InputTest, EventHandlingTerminateMenu) {
    ftxui::Event event = ftxui::Event::Character("\x14");
    bool event_handled = input.get_input_component()->OnEvent(event);

    EXPECT_TRUE(event_handled);
    EXPECT_FALSE(GameState::game_session_in_progress);
}


TEST_F(InputTest, EventHandlingTerminateRefresh) {
    ftxui::Event event = ftxui::Event::Character("\x12");
    bool event_handled = input.get_input_component()->OnEvent(event);

    EXPECT_TRUE(event_handled);
    EXPECT_TRUE(GameState::refresh_session);
}