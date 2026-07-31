//
// Created by Kristijan Zalac on 3/8/25.
//

#ifndef INPUT_H
#define INPUT_H

#include "../engines/InputLineEngine.h"
#include "../engines/InputWordCountEngine.h"
#include "Text.h"
#include "ftxui/component/component.hpp"

class Input {
public:
    explicit Input(const Text& text_instance);
    [[nodiscard]] ftxui::Component get_input_component();
    [[nodiscard]] ftxui::Component get_accuracy_component() const;
    [[nodiscard]] const int& get_word_count_reference() const;

private:
    std::string input_text_;
    ftxui::Component input_component_;
    InputWordCountEngine input_word_count_;
    InputLineEngine input_line_;
};


#endif  // INPUT_H
