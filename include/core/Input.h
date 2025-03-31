//
// Created by kil3 on 3/8/25.
//

#ifndef INPUT_H
#define INPUT_H

#include "gtest/gtest_prod.h"
#include "ftxui/component/component.hpp"
#include "Text.h"


class Input {
    public:
        explicit Input(std::shared_ptr<Text> text_instance);
        ftxui::Component get_input_component();
        [[nodiscard]] ftxui::Component get_accuracy_component() const;
        [[nodiscard]] const int& get_word_count_reference() const;
        [[nodiscard]] float get_percentage_of_correct_input() const;
    private:
        int word_count_;
        int current_line_index_;
        std::string input_text_;
        ftxui::Component input_component_;
        std::shared_ptr<Text> text_instance_;
        ftxui::Elements current_input_line_;
        std::vector<ftxui::Elements> previous_input_lines_;
        ftxui::Elements total_input_lines_;
        std::vector<bool> character_accuracy_;

        void add_element();
        void remove_element();
        void go_to_new_line();
        void render_input_text();
        void go_to_previous_line();
        void set_amount_of_words();
        [[nodiscard]] bool should_add_element() const;
        [[nodiscard]] bool should_remove_element() const;
        [[nodiscard]] bool should_go_to_previous_line() const;
        [[nodiscard]] bool should_go_to_next_line() const;
        [[nodiscard]] int get_previous_lines_size() const;
        [[nodiscard]] ftxui::Element get_next_character();
        [[nodiscard]] ftxui::Element get_accuracy_element_() const;

        FRIEND_TEST(InputTest, Initialization);
        FRIEND_TEST(InputTestMultiLine, LineTransitionOnSpace);
        FRIEND_TEST(InputTestMultiLine, BackspaceAtLineStart);
        FRIEND_TEST(InputTestMultiLine, ShouldGoToNextLine);
        FRIEND_TEST(InputTestMultiLine, ShouldNotGoToNextLine);
        FRIEND_TEST(InputTestMultiLine, ShouldNotGoToNextLineLastLine);
        FRIEND_TEST(InputTestMultiLine, ShouldGoToPreviousLine);
        FRIEND_TEST(InputTestMultiLine, ShouldNotGoToPreviousLine);
        FRIEND_TEST(InputTestMultiLine, ShouldNotGoToPreviousLineBeforeGoingToNext);
        FRIEND_TEST(InputTestMultiLine, ShouldNotAddOrRemoveElement);
        FRIEND_TEST(InputTestMultiLine, ShouldAddElement);
        FRIEND_TEST(InputTestMultiLine, AddElementGameFinish);
        FRIEND_TEST(InputTestMultiLine, ShouldRemoveElement);
        FRIEND_TEST(InputTestMultiLine, ShouldAddAndRemoveElement);
        FRIEND_TEST(InputTestMultiLine, PreviousLinesSizeWhenNothingInputed);
        FRIEND_TEST(InputTestMultiLine, PreviousLinesSizeWhenFirstLine);
        FRIEND_TEST(InputTestMultiLine, PreviousLinesSizeWhenSecondLine);
        FRIEND_TEST(InputTestMultiLine, PreviousLinesSize);
        FRIEND_TEST(InputTest, SetWordsNone);
        FRIEND_TEST(InputTest, SetWordsOne);
        FRIEND_TEST(InputTest, SetWordsOneSpaceAround);
        FRIEND_TEST(InputTest, SetWordsOneSpaceAfter);
        FRIEND_TEST(InputTest, SetWordsOneSpaceBefore);
        FRIEND_TEST(InputTest, SetWordsTwo);
        FRIEND_TEST(InputTest, SetWordsTwoWithTwoSpacesBetween);
        FRIEND_TEST(InputTest, SetWordsTwoWithMultipleSpacesBetweenAndAround);
        FRIEND_TEST(InputTest, SetWordsTwoNewLine);
        FRIEND_TEST(InputTest, SetWordsWordCountCalculation);
        FRIEND_TEST(InputTestMultiLine, PercentageOfCorrectInput);
};



#endif //INPUT_H
