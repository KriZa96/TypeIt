//
// Created by kriza on 01/04/2025.
//

#include <gtest/gtest.h>
#include "../include/core/Text.h"
#include "../include/data/GameState.h"
#include "../include/data/FocusPosition.h"
#include "../include/core/Input.h"


class InputLineTest : public ::testing::Test {
protected:
    InputLineTest()
        : text("line1\nline 2\nThrid line\nsecond last line\nfinally last line."),
        input(text) {}

    void SetUp() override {
        GameState::game_session_in_progress = true;
        GameState::game_finished = false;
        GameState::refresh_session = false;
        FocusPosition::x = 0;
        FocusPosition::y = 0;
    }

    Text text;
    InputLineEngine input;
};


TEST_F(InputLineTest, Initialization) {
    EXPECT_EQ(input.get_current_line_index(), 0);
    EXPECT_EQ(input.get_total_input_lines().size(), text.get_text_lines_size()+1);
}


TEST_F(InputLineTest, LineTransitionOnSpace) {
    EXPECT_EQ(text.get_text_lines_size(), 5);

    int i = 0;
    for (const auto& letter : std::string("line1 ")) {
        input.render_input_text(letter, ++i);
    }

    EXPECT_EQ(input.get_current_line_index(), 1);
    EXPECT_EQ(FocusPosition::y, 1);
}


TEST_F(InputLineTest, BackspaceAtLineStart) {
    int i = 0;
    for (const auto& letter: std::string("line1 ")) {
        input.render_input_text(letter, ++i);
    }

    EXPECT_EQ(input.get_current_line_index(), 1);
    EXPECT_EQ(FocusPosition::y, 1);

    input.render_input_text(' ', --i);

    EXPECT_EQ(input.get_current_line_index(), 0);
    EXPECT_EQ(FocusPosition::y, 0);
}


TEST_F(InputLineTest, ShouldGoToNextLine) {
    int i = 0;
    for (const auto& letter: std::string("line1 ")) {
        input.render_input_text(letter, ++i);
    }

    EXPECT_EQ(input.get_current_line_index(), 1);
}


TEST_F(InputLineTest, ShouldNotGoToNextLine) {
    int i = 0;
    for (const auto& letter: std::string("line1")) {
        input.render_input_text(letter, ++i);
    }

    EXPECT_EQ(input.get_current_line_index(), 0);
}


TEST_F(InputLineTest, DoNothingWhenNoElements) {
    input.render_input_text(' ', 0);
    EXPECT_EQ(input.get_current_line_index(), 0);
}=


TEST_F(InputLineTest, ShouldNotGoToNextLineLastLine) {
    int i = 0;
    for (const auto& letter: std::string("line1 line 2 Thrid line second last line finally last line.J ")) {
        input.render_input_text(letter, ++i);
    }

    EXPECT_EQ(input.get_current_line_index(), 4);
}


TEST_F(InputLineTest, FinishGameOnFullInput) {
    int i = 0;
    for (const auto& letter: std::string("line1 line 2 Thrid line second last line finally last line. ")) {
        input.render_input_text(letter, ++i);
    }

    EXPECT_TRUE(GameState::game_finished);
}



TEST_F(InputLineTest, ShouldGoToPreviousLine) {
    int i = 0;
    for (const auto& letter: std::string("line1 ")) {
        input.render_input_text(letter, ++i);
    }
    ASSERT_EQ(input.get_current_line_index(), 1);

    input.render_input_text(' ', --i);

    EXPECT_EQ(input.get_current_line_index(), 0);
}


TEST_F(InputLineTest, ShouldRemoveFirstElement) {
    input.render_input_text('a', 1);

    ASSERT_EQ(input.get_current_line_size(), 1);

    input.render_input_text(' ', 0);

    EXPECT_EQ(input.get_current_line_size(), 0);
}


TEST_F(InputLineTest, ShouldNotGoToPreviousLine) {
    int i = 0;
    for (const auto& letter: std::string("line1 l")) {
        input.render_input_text(letter, ++i);
    }
    ASSERT_EQ(input.get_current_line_index(), 1);

    input.render_input_text(' ', --i);

    EXPECT_EQ(input.get_current_line_index(), 1);
}


TEST_F(InputLineTest, ShouldNotAddOrRemoveElement) {
    int i = 0;
    for (const auto& letter: std::string("lin")) {
        input.render_input_text(letter, ++i);
    }

    input.render_input_text('h', i);
    input.render_input_text('e', i);
    input.render_input_text('l', i);
    input.render_input_text('l', i);
    input.render_input_text('o', i);

    EXPECT_EQ(input.get_current_line_size(), i);
}