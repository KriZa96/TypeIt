//
// Created by kil3 on 3/8/25.
//
#include <gtest/gtest.h>

#include "../include/core/WordCalculator.h"


TEST(WordCalculatorTest, TestMemberVariables) {
    int elapsed_time = 60;
    int word_count = 60;

    WordCalculator word_calculator(elapsed_time, word_count);

    ASSERT_EQ(word_calculator.word_count_, word_count);
    ASSERT_EQ(word_calculator.elapsed_time_, elapsed_time);
}


TEST(WordCalculatorTest, TestWordsPerMinute60Words) {
    int elapsed_time = 60;
    int word_count = 60;

    WordCalculator word_calculator(elapsed_time, word_count);

    ASSERT_EQ(word_calculator.calculate_words_per_minute(), 60);
}


TEST(WordCalculatorTest, TestWordsPerMinuteAbove60Words) {
    int elapsed_time = 60;
    int word_count = 120;

    WordCalculator word_calculator(elapsed_time, word_count);

    ASSERT_EQ(word_calculator.calculate_words_per_minute(), 120);
}


TEST(WordCalculatorTest, TestWordsPerMinuteBelow60Words) {
    int elapsed_time = 60;
    int word_count = 30;

    WordCalculator word_calculator(elapsed_time, word_count);

    ASSERT_EQ(word_calculator.calculate_words_per_minute(), 30);
}


TEST(WordCalculatorTest, TestWordsPerMinuteElapsedTime0) {
    int elapsed_time = 0;
    int word_count = 0;

    WordCalculator word_calculator(elapsed_time, word_count);

    ASSERT_EQ(word_calculator.calculate_words_per_minute(), 0);
}


TEST(WordCalculatorTest, TestGetWordsPerMinuteString) {
    int elapsed_time = 60;
    int word_count = 60;

    WordCalculator word_calculator(elapsed_time, word_count);

    ASSERT_EQ(word_calculator.get_words_per_minute_string(), "wpm: 60");
}


TEST(WordCalculatorTest, TestWordsPerMinuteStringElapsedTime0) {
    int elapsed_time = 0;
    int word_count = 0;

    WordCalculator word_calculator(elapsed_time, word_count);

    ASSERT_EQ(word_calculator.get_words_per_minute_string(), "wpm: 0");
}