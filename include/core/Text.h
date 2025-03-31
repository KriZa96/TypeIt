//
// Created by kil3 on 3/8/25.
//

#ifndef TEXT_H
#define TEXT_H

#include <memory>
#include <vector>

#include "gtest/gtest_prod.h"
#include "ftxui/component/component.hpp"


class Text {
    public:
        explicit Text(std::string text);
        [[nodiscard]] ftxui::Component get_text_component() const;
        [[nodiscard]] std::string get_text() const;
        [[nodiscard]] int get_text_lines_size() const;
        [[nodiscard]] int get_text_line_size(int index) const;
        [[nodiscard]] char get_char_at_line_and_position(int line_index, int char_index) const;
    private:
        std::string text_;
        std::vector<std::string> text_lines_strings_;
        ftxui::Elements text_lines_;
        std::vector<int> text_lines_size_;

        void populate_text_lines();
        [[nodiscard]] ftxui::Element get_text_element() const;
        void push_line(ftxui::Elements& line, std::string& text, int& num_of_characters);

        FRIEND_TEST(TextTest, TestTextEmpty);
        FRIEND_TEST(TextTest, TestTextNotEmpty);
        FRIEND_TEST(TextTest, TestWithnewlineChar);
        FRIEND_TEST(TextTest, TestWithOnlyNewlineChar);
        FRIEND_TEST(TextTest, TestWithNewlineCharAndOneChar);
        FRIEND_TEST(TextTest, TestWithSpacesOneLine);
        FRIEND_TEST(TextTest, TestWithSpacesTwoLine);
        FRIEND_TEST(TextTest, TestWithSpacesThreeLines);
        FRIEND_TEST(TextTest, TestWithSpacesAndNewLineThreeLines);
};


#endif //TEXT_H
