//
// Created by kil3 on 3/8/25.
//
#include <gtest/gtest.h>

#include "../include/engines/WordCalculatorEngine.h"


TEST(WordCalculatorTest, TestWordsPerMinute60Words) {
    int elapsed_time = 60;
    int word_count = 60;

    WordCalculatorEngine word_calculator;

    ASSERT_EQ(word_calculator.get_words_per_minute_string(elapsed_time, word_count), "wpm: 60");
}


TEST(WordCalculatorTest, TestWordsPerMinuteAbove60Words) {
    int elapsed_time = 60;
    int word_count = 120;

    WordCalculatorEngine word_calculator;

    ASSERT_EQ(word_calculator.get_words_per_minute_string(elapsed_time, word_count), "wpm: 120");
}


TEST(WordCalculatorTest, TestWordsPerMinuteBelow60Words) {
    int elapsed_time = 60;
    int word_count = 30;

    WordCalculatorEngine word_calculator;

    ASSERT_EQ(word_calculator.get_words_per_minute_string(elapsed_time, word_count), "wpm: 30");
}


TEST(WordCalculatorTest, TestWordsPerMinuteElapsedTime0) {
    int elapsed_time = 0;
    int word_count = 0;

    WordCalculatorEngine word_calculator;

    ASSERT_EQ(word_calculator.get_words_per_minute_string(elapsed_time, word_count), "wpm: 0");
}
