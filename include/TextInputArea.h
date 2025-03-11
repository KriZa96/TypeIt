//
// Created by kil3 on 3/9/25.
//

#ifndef TEXTINPUTAREA_H
#define TEXTINPUTAREA_H


#include "ftxui/component/component.hpp"


class TextInputArea {
    public:
        TextInputArea(ftxui::Component input_component, ftxui::Component text_component);
        [[nodiscard]] ftxui::Component get_text_input_component() const;
    private:
        ftxui::Component input_component_;
        ftxui::Component text_component_;
        ftxui::FlexboxConfig config_;
};


#endif //TEXTINPUTAREA_H
