//
// Created by kil3 on 3/9/25.
//

#include <gtest/gtest.h>
#include "../include/core/Text.h"
#include "../include/data/GameState.h"
#include "../include/data/FocusPosition.h"

#define private public
#include "../include/core/Input.h"
#undef private


class InputTest : public ::testing::Test {
protected:
    InputTest()
        : text(std::make_shared<Text>("line1\nline2")),
          input(text) {}

    void SetUp() override {
        GameState::game_session_in_progress = true;
        GameState::game_finished = false;
        GameState::refresh_session = false;
        FocusPosition::x = 0;
        FocusPosition::y = 0;
    }

    std::shared_ptr<Text> text;
    Input input;
};


class InputTestMultiLine : public ::testing::Test {
protected:
    InputTestMultiLine()
        : text(std::make_shared<Text>("line1\nline 2\nThrid line\nsecond last line\nfinally last line.")),
          input(text) {}

    void SetUp() override {
        GameState::game_session_in_progress = true;
        GameState::game_finished = false;
        GameState::refresh_session = false;
        FocusPosition::x = 0;
        FocusPosition::y = 0;
    }

    std::shared_ptr<Text> text;
    Input input;
};

TEST_F(InputTest, Initialization) {
    EXPECT_EQ(input.get_word_count_reference(), 3);
    EXPECT_EQ(input.current_line_index_, 0);
    EXPECT_TRUE(input.input_text_.empty());
    EXPECT_EQ(input.total_input_lines_.size(), text->get_text_lines_size()+1);
}


TEST_F(InputTestMultiLine, LineTransitionOnSpace) {
    EXPECT_EQ(text->get_text_lines_size(), 5);

    for (const auto& letter : std::string("line1 ")) {
        input.input_text_ += letter;
        input.render_input_text();
    }

    EXPECT_EQ(input.current_line_index_, 1);
    EXPECT_EQ(FocusPosition::y, 1);
}


TEST_F(InputTestMultiLine, BackspaceAtLineStart) {
    for (const auto& letter: std::string("line1 ")) {
        input.input_text_ += letter;
        input.render_input_text();
    }
    EXPECT_EQ(input.current_line_index_, 1);
    EXPECT_EQ(FocusPosition::y, 1);

    input.input_text_.pop_back();
    input.render_input_text();

    EXPECT_EQ(input.current_line_index_, 0);
    EXPECT_EQ(FocusPosition::y, 0);
}


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


TEST_F(InputTestMultiLine, ShouldGoToNextLine) {
    for (const auto& letter: std::string("line1")) {
        input.input_text_ += letter;
        input.render_input_text();
    }

    input.input_text_ += " ";
    input.current_input_line_.push_back(ftxui::text(L" "));

    EXPECT_TRUE(input.should_go_to_next_line());
}

TEST_F(InputTestMultiLine, ShouldNotGoToNextLine) {
    for (const auto& letter: std::string("line")) {
        input.input_text_ += letter;
        input.render_input_text();
    }

    input.input_text_ += " ";
    input.current_input_line_.push_back(ftxui::text(L" "));

    EXPECT_FALSE(input.should_go_to_next_line());
}


TEST_F(InputTestMultiLine, ShouldNotGoToNextLineLastLine) {
    for (const auto& letter: std::string("line1 line 2 Thrid line second last line finally last line.J")) {
        input.input_text_ += letter;
        input.render_input_text();
    }

    input.input_text_ += " ";
    input.current_input_line_.push_back(ftxui::text(L" "));

    EXPECT_FALSE(input.should_go_to_next_line());
}


TEST_F(InputTestMultiLine, ShouldGoToPreviousLine) {
    for (const auto& letter: std::string("line1 ")) {
        input.input_text_ += letter;
        input.render_input_text();
    }
    ASSERT_EQ(input.current_line_index_, 1);

    input.input_text_.pop_back();

    EXPECT_TRUE(input.should_go_to_previous_line());
}


TEST_F(InputTestMultiLine, ShouldNotGoToPreviousLine) {
    for (const auto& letter: std::string("line1 l")) {
        input.input_text_ += letter;
        input.render_input_text();
    }

    input.input_text_.pop_back();

    EXPECT_FALSE(input.should_go_to_previous_line());
}


TEST_F(InputTestMultiLine, ShouldNotGoToPreviousLineBeforeGoingToNext) {
    for (const auto& letter: std::string("line1")) {
        input.input_text_ += letter;
        input.render_input_text();
    }

    input.input_text_.pop_back();

    EXPECT_FALSE(input.should_go_to_previous_line());
}


TEST_F(InputTestMultiLine, ShouldNotAddOrRemoveElement) {
    EXPECT_FALSE(input.should_remove_element());
    EXPECT_FALSE(input.should_add_element());
}


TEST_F(InputTestMultiLine, ShouldAddElement) {
    input.input_text_ = "l";
    EXPECT_FALSE(input.should_remove_element());
    EXPECT_TRUE(input.should_add_element());
}


TEST_F(InputTestMultiLine, ShouldRemoveElement) {
    input.input_text_ = "l";
    input.render_input_text();

    input.input_text_ = "";
    EXPECT_TRUE(input.should_remove_element());
    EXPECT_FALSE(input.should_add_element());
}


TEST_F(InputTestMultiLine, ShouldAddAndRemoveElement) {
    std::string full_text = "line1 line 2 Thrid line second last line finally last line.";
    for (const auto& letter: full_text) {
        input.input_text_ += letter;
        EXPECT_TRUE(input.should_add_element());
        input.render_input_text();
    }

    for (int i = 0; i < full_text.size(); ++i) {
        input.input_text_.pop_back();
        EXPECT_TRUE(input.should_remove_element());
        input.render_input_text();
    }
}


TEST_F(InputTestMultiLine, PreviousLinesSizeWhenNothingInputed) {
    EXPECT_EQ(input.get_previous_lines_size(), 0);
}


TEST_F(InputTestMultiLine, PreviousLinesSizeWhenFirstLine) {
    for (const auto& letter: "line1") {
        input.input_text_ += letter;
        input.render_input_text();
    }

    EXPECT_EQ(input.get_previous_lines_size(), 0);
}


TEST_F(InputTestMultiLine, PreviousLinesSizeWhenSecondLine) {
    for (const auto& letter: std::string("line1 ")) {
        input.input_text_ += letter;
        input.render_input_text();
    }

    EXPECT_EQ(input.get_previous_lines_size(), 6);
}


TEST_F(InputTestMultiLine, PreviousLinesSize) {
    for (const auto& letter: std::string("line1 line 2 Thrid line second last line")) {
        input.input_text_ += letter;
        input.render_input_text();
    }

    EXPECT_EQ(input.get_previous_lines_size(), 24);
}


TEST_F(InputTest, SetWordsNone) {
    input.input_text_ = "";
    input.set_amount_of_words();

    EXPECT_EQ(input.word_count_, 0);
}


TEST_F(InputTest, SetWordsOne) {
    input.input_text_ = "one";
    input.set_amount_of_words();

    EXPECT_EQ(input.word_count_, 1);
}


TEST_F(InputTest, SetWordsOneSpaceAround) {
    input.input_text_ = " one ";
    input.set_amount_of_words();

    EXPECT_EQ(input.word_count_, 1);
}


TEST_F(InputTest, SetWordsOneSpaceAfter) {
    input.input_text_ = "one ";
    input.set_amount_of_words();

    EXPECT_EQ(input.word_count_, 1);
}


TEST_F(InputTest, SetWordsOneSpaceBefore) {
    input.input_text_ = " one";
    input.set_amount_of_words();

    EXPECT_EQ(input.word_count_, 1);
}


TEST_F(InputTest, SetWordsTwo) {
    input.input_text_ = "one two";
    input.set_amount_of_words();

    EXPECT_EQ(input.word_count_, 2);
}


TEST_F(InputTest, SetWordsTwoWithTwoSpacesBetween) {
    input.input_text_ = "one  two";
    input.set_amount_of_words();

    EXPECT_EQ(input.word_count_, 2);
}


TEST_F(InputTest, SetWordsTwoWithMultipleSpacesBetweenAndAround) {
    input.input_text_ = "  one     two   ";
    input.set_amount_of_words();

    EXPECT_EQ(input.word_count_, 2);
}


TEST_F(InputTest, SetWordsTwoNewLine) {
    input.input_text_ = "one\ntwo";
    input.set_amount_of_words();

    EXPECT_EQ(input.word_count_, 2);
}


TEST_F(InputTest, SetWordsWordCountCalculation) {
    const int& word_count = input.get_word_count_reference();

    input.input_text_ = "one two three";
    input.set_amount_of_words();
    EXPECT_EQ(word_count, 3);

    input.input_text_ = "   leading spaces";
    input.set_amount_of_words();
    EXPECT_EQ(word_count, 2);
}


TEST_F(InputTestMultiLine, PercentageOfCorrectInput) {
    for (const auto& letter: std::string("line1 line 2 Thrid line second last line finally last line.")) {
        input.input_text_ += letter;
        input.render_input_text();
    }

    EXPECT_EQ(input.get_percentage_of_correct_input(), 100.);
}
