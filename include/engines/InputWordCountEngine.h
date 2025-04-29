//
// Created by Kristijan Zalac on 31/03/2025.
//

#ifndef TYPEIT_INPUTWORDCOUNTENGINE_H
#define TYPEIT_INPUTWORDCOUNTENGINE_H

#include <string>

class InputWordCountEngine {
public:
    InputWordCountEngine(): word_count_(0) {};
    void set_word_count(const std::string& input_text);
    [[nodiscard]] const int& get_word_count_reference() const;
private:
    int word_count_;
};


#endif  // TYPEIT_INPUTWORDCOUNTENGINE_H
