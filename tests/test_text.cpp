//
// Created by kil3 on 3/8/25.
//
#include <gtest/gtest.h>

#include "../include/core/Text.h"


TEST(TextTest, TestTextEmpty) {
    Text text("");

    EXPECT_TRUE(text.get_text_lines().empty());
}


TEST(TextTest, TestTextNotEmpty) {
    Text text("Hello");

    EXPECT_FALSE(text.get_text_lines().empty());
}


TEST(TextTest, TestWithnewlineChar) {
    Text text("Hello\nNo\nHello");

    ASSERT_EQ(text.get_text_lines_size(), 3);
}


TEST(TextTest, TestWithOnlyNewlineChar) {
    Text text("\n\n");

    ASSERT_EQ(text.get_text_lines_size(), 2);
}


TEST(TextTest, TestWithNewlineCharAndOneChar) {
    Text text("\n\nA");

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
    std::string text_ = "Lorem ipsum dolor sit amet, consectetur adipiscing elit. Nulla convallis, urna id fringilla volutpat, sapien justo tincidunt urna.";

    Text text(text_);

    ASSERT_EQ(text.get_text_lines_size(), 3);
}


TEST(TextTest, TestWithSpacesAndNewLineThreeLines) {
    std::string text_ = "Lorem ipsum dolor sit amet,\nconsectetur adipiscing elit. Nulla convallis,\n urna id fringilla volutpat, sapien justo tincidunt urna.";

    Text text(text_);

    ASSERT_EQ(text.get_text_lines_size(), 3);
}
