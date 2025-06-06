//
// Created by Kristijan Zalac on 31/03/2025.
//

#ifndef TYPEIT_WORDCALCULATORENGINE_H
#define TYPEIT_WORDCALCULATORENGINE_H

#include <string>

class WordCalculatorEngine {
public:
    [[nodiscard]] static std::string get_words_per_minute_string(const int& elapsed_time, const int& word_count) ;
private:
    [[nodiscard]] static int calculate_words_per_minute(const int& elapsed_time, const int& word_count) ;
};


#endif  // TYPEIT_WORDCALCULATORENGINE_H
