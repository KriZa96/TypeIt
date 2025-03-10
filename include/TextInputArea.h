//
// Created by kil3 on 3/9/25.
//

#ifndef TEXTINPUTAREA_H
#define TEXTINPUTAREA_H

#include "ftxui/component/component.hpp"

#include "Text.h"
#include "Input.h"


class TextInputArea {
    public:
        TextInputArea(std::string text);
        ftxui::Component get_text_input_component();
        std::shared_ptr<int> get_word_count_shared_pointer();
    private:
        Text text_;
        Input input_;
        ftxui::Component input_component_;
        ftxui::FlexboxConfig config_;
};


#endif //TEXTINPUTAREA_H
