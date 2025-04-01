//
// Created by kil3 on 3/8/25.
//

#include "../../include/core/WordCalculator.h"
#include "../../include/engines/WordCalculatorEngine.h"


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


ftxui::Element WordCalculator::get_word_per_minute_element() const {
    return ftxui::text(WordCalculatorEngine::get_words_per_minute_string(elapsed_time_, word_count_));
}
