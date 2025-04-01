//
// Created by kriza on 31/03/2025.
//

#include <format>

#include "../../include/engines/WordCalculatorEngine.h"


std::string WordCalculatorEngine::get_words_per_minute_string(const int& elapsed_time, const int& word_count) {
    return std::format("wpm: {}", calculate_words_per_minute(elapsed_time, word_count));
}


int WordCalculatorEngine::calculate_words_per_minute(const int& elapsed_time, const int& word_count) {
    if (elapsed_time <= 1) {
        return 0;
    }
    return static_cast<int>(60. * word_count / elapsed_time);
}