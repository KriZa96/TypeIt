//
// Created by kriza on 31/03/2025.
//

#include "../../include/engines/InputWordCountEngine.h"

#include <iterator>
#include <sstream>

void InputWordCountEngine::set_word_count(const std::string& input_text) {
    std::istringstream stream(input_text);
    word_count_ = static_cast<int>(
            std::distance(
                std::istream_iterator<std::string>(stream),
                std::istream_iterator<std::string>()
            )
    );
}

const int& InputWordCountEngine::get_word_count_reference() const {
    return word_count_;
}