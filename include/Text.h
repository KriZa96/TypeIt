//
// Created by kil3 on 3/8/25.
//

#ifndef TEXT_H
#define TEXT_H

#include <memory>
#include <vector>
#include "ITextSource.h"
#include "ftxui/component/component.hpp"


class Text {
    public:
        Text(std::string text, std::shared_ptr<int> position_x, std::shared_ptr<int> position_y);
        ftxui::Element get_text_element();
    private:
        std::string text_;
        std::shared_ptr<int> position_x_;
        std::shared_ptr<int> position_y_;
        ftxui::Elements text_lines_;

        void populate_text_lines();
        void push_line(ftxui::Elements& line, int& num_of_characters);
};



#endif //TEXT_H
