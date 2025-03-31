//
// Created by kil3 on 3/8/25.
//

#ifndef WORDCALCULATOR_H
#define WORDCALCULATOR_H


#include "gtest/gtest_prod.h"
#include "ftxui/component/component.hpp"


class WordCalculator {
    public:
        WordCalculator(const int& elapsed_time, const int& word_count);
        [[nodiscard]] ftxui::Component get_word_calculator_component() const;
    private:
        const int& elapsed_time_;
        const int& word_count_;

        [[nodiscard]] int calculate_words_per_minute() const;
        [[nodiscard]] ftxui::Element get_word_per_minute_element() const;
        [[nodiscard]] std::string get_words_per_minute_string() const;

        FRIEND_TEST(WordCalculatorTest, TestMemberVariables);
        FRIEND_TEST(WordCalculatorTest, TestWordsPerMinute60Words);
        FRIEND_TEST(WordCalculatorTest, TestWordsPerMinuteAbove60Words);
        FRIEND_TEST(WordCalculatorTest, TestWordsPerMinuteBelow60Words);
        FRIEND_TEST(WordCalculatorTest, TestWordsPerMinuteElapsedTime0);
        FRIEND_TEST(WordCalculatorTest, TestGetWordsPerMinuteString);
        FRIEND_TEST(WordCalculatorTest, TestWordsPerMinuteStringElapsedTime0);
};



#endif //WORDCALCULATOR_H
