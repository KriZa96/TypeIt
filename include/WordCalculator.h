//
// Created by kil3 on 3/8/25.
//

#ifndef WORDCALCULATOR_H
#define WORDCALCULATOR_H

#include "ftxui/component/component.hpp"


class WordCalculator {
    public:
        WordCalculator(std::shared_ptr<int> elapsed_time, std::shared_ptr<int> word_count);
        int calculate_words_per_minute();
        ftxui::Element get_word_per_minute_element();
    private:
        std::shared_ptr<int> elapsed_time_;
        std::shared_ptr<int> word_count_;

        std::string get_words_per_minute_string();
};



#endif //WORDCALCULATOR_H
