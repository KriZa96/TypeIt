//
// Created by Kristijan Zalac on 31/03/2025.
//
#include <gtest/gtest.h>

#include "../include/engines/InputWordCountEngine.h"


class InputWordCountTest : public ::testing::Test {
protected:
    InputWordCountTest(): input_word_count() {};
    InputWordCountEngine input_word_count;
};


TEST_F(InputWordCountTest, SetWordsNone) {
    input_word_count.set_word_count("");

    EXPECT_EQ(input_word_count.get_word_count_reference(), 0);
}


TEST_F(InputWordCountTest, SetWordsOne) {
    input_word_count.set_word_count("one");

    EXPECT_EQ(input_word_count.get_word_count_reference(), 1);
}


TEST_F(InputWordCountTest, SetWordsOneSpaceAround) {
    input_word_count.set_word_count(" one ");

    EXPECT_EQ(input_word_count.get_word_count_reference(), 1);
}


TEST_F(InputWordCountTest, SetWordsOneSpaceAfter) {
    input_word_count.set_word_count("one ");

    EXPECT_EQ(input_word_count.get_word_count_reference(), 1);
}


TEST_F(InputWordCountTest, SetWordsOneSpaceBefore) {
    input_word_count.set_word_count(" one");

    EXPECT_EQ(input_word_count.get_word_count_reference(), 1);
}


TEST_F(InputWordCountTest, SetWordsTwo) {
    input_word_count.set_word_count("one two");

    EXPECT_EQ(input_word_count.get_word_count_reference(), 2);
}


TEST_F(InputWordCountTest, SetWordsTwoWithTwoSpacesBetween) {
    input_word_count.set_word_count("one  two");

    EXPECT_EQ(input_word_count.get_word_count_reference(), 2);
}


TEST_F(InputWordCountTest, SetWordsTwoWithMultipleSpacesBetweenAndAround) {
    input_word_count.set_word_count("  one     two   ");

    EXPECT_EQ(input_word_count.get_word_count_reference(), 2);
}


TEST_F(InputWordCountTest, SetWordsTwoNewLine) {
    input_word_count.set_word_count("one\ntwo");

    EXPECT_EQ(input_word_count.get_word_count_reference(), 2);
}


TEST_F(InputWordCountTest, SetWordsWordCountCalculation) {
    const int& word_count = input_word_count.get_word_count_reference();

    input_word_count.set_word_count("one two three");
    EXPECT_EQ(input_word_count.get_word_count_reference(), 3);

    input_word_count.set_word_count("   leading spaces");
    EXPECT_EQ(word_count, 2);
}
