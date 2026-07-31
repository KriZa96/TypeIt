//
// Created by Kristijan Zalac on 3/8/25.
//
#include <gtest/gtest.h>

#include "../include/core/Text.h"


TEST(TextTest, TestTextEmpty) {
    std::string text_ = "";
    Text text(text_);

    EXPECT_TRUE(text.get_text_lines().empty());
}


TEST(TextTest, TestTextNotEmpty) {
    std::string text_ = "Hello";
    Text text(text_);

    EXPECT_FALSE(text.get_text_lines().empty());
}


TEST(TextTest, TestWithnewlineChar) {
    std::string text_ = "Hello\nNo\nHello";
    Text text(text_);

    ASSERT_EQ(text.get_text_lines_size(), 3);
}


TEST(TextTest, TestWithOnlyNewlineChar) {
    std::string text_ = "\n\n";
    Text text(text_);

    ASSERT_EQ(text.get_text_lines_size(), 2);
}


TEST(TextTest, TestWithNewlineCharAndOneChar) {
    std::string text_ = "\n\nA";
    Text text(text_);

    ASSERT_EQ(text.get_text_lines_size(), 3);
}


TEST(TextTest, TestWithSpacesOneLine) {
    std::string text_ = "Lorem ipsum dolor sit amet, consectetur adipiscing.";

    Text text(text_);

    ASSERT_EQ(text.get_text_lines_size(), 1);
}


TEST(TextTest, TestWithSpacesTwoLine) {
    std::string text_ = "Lorem ipsum dolor sit amet, consectetur adipiscing elit yo.";

    Text text(text_);

    ASSERT_EQ(text.get_text_lines_size(), 2);
}


TEST(TextTest, TestWithSpacesThreeLines) {
    std::string text_ =
            "Lorem ipsum dolor sit amet, consectetur adipiscing elit. Nulla convallis, urna id fringilla volutpat, "
            "sapien justo tincidunt urna.";

    Text text(text_);

    ASSERT_EQ(text.get_text_lines_size(), 3);
}


TEST(TextTest, TestWithSpacesAndNewLineThreeLines) {
    std::string text_ =
            "Lorem ipsum dolor sit amet,\nconsectetur adipiscing elit. Nulla convallis,\n urna id fringilla volutpat, "
            "sapien justo tincidunt urna.";

    Text text(text_);

    ASSERT_EQ(text.get_text_lines_size(), 3);
}


// TI-004 parenthesised the wrap condition in Text::populate_text_lines. The
// grouping was already what the compiler picked -- `&&` binds tighter than
// `||` -- so this pins the line counts across that edit, on the same inputs
// the tests above use plus the boundary cases the condition actually turns on.
TEST(TextTest, WrapConditionUnchangedAfterParenthesisation) {
    const std::vector<std::pair<std::string, int>> cases = {
            {"", 0},
            {"Hello", 1},
            {"Hello\nNo\nHello", 3},
            {"\n\n", 2},
            {"\n\nA", 3},
            {"Lorem ipsum dolor sit amet, consectetur adipiscing.", 1},
            {"Lorem ipsum dolor sit amet, consectetur adipiscing elit yo.", 2},
            {"Lorem ipsum dolor sit amet, consectetur adipiscing elit. Nulla convallis, urna id fringilla volutpat, "
             "sapien justo tincidunt urna.",
             3},
            {"Lorem ipsum dolor sit amet,\nconsectetur adipiscing elit. Nulla convallis,\n urna id fringilla volutpat, "
             "sapien justo tincidunt urna.",
             3},
            // The wrap threshold itself: a space is the 54th element on the
            // line and does not wrap, the 55th does. Both counts verified
            // against the pre-parenthesisation code, not assumed.
            {std::string(53, 'a') + " b", 1},
            {std::string(54, 'a') + " b", 2},
            {" ", 1},
            {"a b ", 1},
            {"\n", 1},
    };

    for (const auto& [input, expected_lines]: cases) {
        Text text(input);
        EXPECT_EQ(text.get_text_lines_size(), expected_lines) << "input: " << input;
    }
}
