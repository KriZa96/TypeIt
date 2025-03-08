//
// Created by kil3 on 3/8/25.
//

#include "../../include/WordCalculator.h"


WordCalculator::WordCalculator(std::shared_ptr<float> elapsed_time, std::shared_ptr<float> word_count):
    elapsed_time_(elapsed_time),
    word_count_(word_count) {}


int WordCalculator::calculate_words_per_minute() {
    if (*elapsed_time_ <= 0) {
        return 0;
    }
    return *word_count_ / *elapsed_time_ * 60;
}


ftxui::Element WordCalculator::get_word_per_minute_element() {
    return ftxui::text(get_words_per_minute_string());
}


std::string WordCalculator::get_words_per_minute_string(){
    return std::format("{} wpm", calculate_words_per_minute());
}