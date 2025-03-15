//
// Created by kil3 on 3/8/25.
//

#ifndef WORDCALCULATOR_H
#define WORDCALCULATOR_H

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
};



#endif //WORDCALCULATOR_H
