//
// Created by kil3 on 3/8/25.
//

#include "../../include/core/WordCalculator.h"


WordCalculator::WordCalculator(const int& elapsed_time, const int& word_count):
    elapsed_time_(elapsed_time),
    word_count_(word_count) {}


ftxui::Component WordCalculator::get_word_calculator_component() const {
    return ftxui::Renderer(
        [this] {
            return get_word_per_minute_element();
        }
    );
}


int WordCalculator::calculate_words_per_minute() const {
    if (elapsed_time_ <= 1) {
        return 0;
    }
    return static_cast<int>(60. * word_count_ / elapsed_time_);
}


ftxui::Element WordCalculator::get_word_per_minute_element() const {
    return ftxui::text(get_words_per_minute_string());
}


std::string WordCalculator::get_words_per_minute_string() const{
    return std::format("wpm: {}", calculate_words_per_minute());
}
