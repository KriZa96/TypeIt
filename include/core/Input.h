//
// Created by kil3 on 3/8/25.
//

#ifndef INPUT_H
#define INPUT_H

#include "ftxui/component/component.hpp"
#include "Text.h"
#include "../engines/InputWordCountEngine.h"
#include "../engines/InputLineEngine.h"

class Input {
    public:
        explicit Input(const Text& text_instance);
        [[nodiscard]] ftxui::Component get_input_component();
        [[nodiscard]] ftxui::Component get_accuracy_component() const;
        [[nodiscard]] const int& get_word_count_reference() const;
    private:
        std::string input_text_;
        ftxui::Component input_component_;
        Text text_instance_;
        InputWordCountEngine input_word_count_;
        InputLineEngine input_line_;
};



#endif //INPUT_H
