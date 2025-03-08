//
// Created by kil3 on 3/8/25.
//
#include <gtest/gtest.h>

#define private public
#include "../include/Text.h"
#undef private


TEST(TextTest, TestTextEmpty) {
    int position_x = 0;
    int position_y = 0;

    Text text("", std::make_shared<int>(position_x), std::make_shared<int>(position_y));

    EXPECT_TRUE(text.text_lines_.empty());
}


TEST(TextTest, TestTextNotEmpty) {
    int position_x = 0;
    int position_y = 0;

    Text text("Hello", std::make_shared<int>(position_x), std::make_shared<int>(position_y));

    EXPECT_FALSE(text.text_lines_.empty());
}


TEST(TextTest, TestWithnewlineChar) {
    int position_x = 0;
    int position_y = 0;

    Text text("Hello\nNo\nHello", std::make_shared<int>(position_x), std::make_shared<int>(position_y));

    ASSERT_EQ(text.text_lines_.size(), 3);
}


TEST(TextTest, TestWithOnlyNewlineChar) {
    int position_x = 0;
    int position_y = 0;

    Text text("\n\n", std::make_shared<int>(position_x), std::make_shared<int>(position_y));

    ASSERT_EQ(text.text_lines_.size(), 2);
}


TEST(TextTest, TestWithNewlineCharAndOneChar) {
    int position_x = 0;
    int position_y = 0;

    Text text("\n\nA", std::make_shared<int>(position_x), std::make_shared<int>(position_y));

    ASSERT_EQ(text.text_lines_.size(), 3);
}


TEST(TextTest, TestWithSpacesOneLine) {
    int position_x = 0;
    int position_y = 0;
    std::string text_ = "Lorem ipsum dolor sit amet, consectetur adipiscing.";

    Text text(text_, std::make_shared<int>(position_x), std::make_shared<int>(position_y));

    ASSERT_EQ(text.text_lines_.size(), 1);
}


TEST(TextTest, TestWithSpacesTwoLine) {
    int position_x = 0;
    int position_y = 0;
    std::string text_ = "Lorem ipsum dolor sit amet, consectetur adipiscing elit yo.";

    Text text(text_, std::make_shared<int>(position_x), std::make_shared<int>(position_y));

    ASSERT_EQ(text.text_lines_.size(), 2);
}


TEST(TextTest, TestWithSpacesThreeLines) {
    int position_x = 0;
    int position_y = 0;
    std::string text_ = "Lorem ipsum dolor sit amet, consectetur adipiscing elit. Nulla convallis, urna id fringilla volutpat, sapien justo tincidunt urna.";

    Text text(text_, std::make_shared<int>(position_x), std::make_shared<int>(position_y));

    ASSERT_EQ(text.text_lines_.size(), 3);
}


TEST(TextTest, TestWithSpacesAndNewLineThreeLines) {
    int position_x = 0;
    int position_y = 0;
    std::string text_ = "Lorem ipsum dolor sit amet,\nconsectetur adipiscing elit. Nulla convallis,\n urna id fringilla volutpat, sapien justo tincidunt urna.";

    Text text(text_, std::make_shared<int>(position_x), std::make_shared<int>(position_y));

    ASSERT_EQ(text.text_lines_.size(), 3);
}
