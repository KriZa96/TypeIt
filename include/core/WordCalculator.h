//
// Created by Kristijan Zalac on 3/8/25.
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

        [[nodiscard]] ftxui::Element get_word_per_minute_element() const;
};


#endif //WORDCALCULATOR_H
