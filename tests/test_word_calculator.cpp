//
// Created by kil3 on 3/8/25.
//
#include <gtest/gtest.h>

#define private public
#include "../include/WordCalculator.h"
#undef private


TEST(WordCalculator, TestMemberVariables) {
    int elapsed_time = 60;
    int word_count = 60;

    WordCalculator word_calculator(std::make_shared<float>(elapsed_time), std::make_shared<float>(word_count));

    ASSERT_EQ(*word_calculator.word_count_, word_count);
    ASSERT_EQ(*word_calculator.elapsed_time_, elapsed_time);
}


TEST(WordCalculator, TestWordsPerMinute60Words) {
    int elapsed_time = 60;
    int word_count = 60;

    WordCalculator word_calculator(std::make_shared<float>(elapsed_time), std::make_shared<float>(word_count));

    ASSERT_EQ(word_calculator.calculate_words_per_minute(), 60);
}


TEST(WordCalculator, TestWordsPerMinuteAbove60Words) {
    int elapsed_time = 60;
    int word_count = 120;

    WordCalculator word_calculator(std::make_shared<float>(elapsed_time), std::make_shared<float>(word_count));

    ASSERT_EQ(word_calculator.calculate_words_per_minute(), 120);
}


TEST(WordCalculator, TestWordsPerMinuteBelow60Words) {
    int elapsed_time = 60;
    int word_count = 30;

    WordCalculator word_calculator(std::make_shared<float>(elapsed_time), std::make_shared<float>(word_count));

    ASSERT_EQ(word_calculator.calculate_words_per_minute(), 30);
}


TEST(WordCalculator, TestWordsPerMinuteElapsedTime0) {
    int elapsed_time = 0;
    int word_count = 0;

    WordCalculator word_calculator(std::make_shared<float>(elapsed_time), std::make_shared<float>(word_count));

    ASSERT_EQ(word_calculator.calculate_words_per_minute(), 0);
}


TEST(WordCalculator, TestGetWordsPerMinuteString) {
    int elapsed_time = 60;
    int word_count = 60;

    WordCalculator word_calculator(std::make_shared<float>(elapsed_time), std::make_shared<float>(word_count));

    ASSERT_EQ(word_calculator.get_words_per_minute_string(), "60 wpm");
}


TEST(WordCalculator, TestWordsPerMinuteStringElapsedTime0) {
    int elapsed_time = 0;
    int word_count = 0;

    WordCalculator word_calculator(std::make_shared<float>(elapsed_time), std::make_shared<float>(word_count));

    ASSERT_EQ(word_calculator.get_words_per_minute_string(), "0 wpm");
}