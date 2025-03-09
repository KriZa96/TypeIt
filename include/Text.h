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
        Text(std::string text);
        ftxui::Element get_text_element();
        int get_text_size() const;
        int get_text_lines_size() const;
        int get_text_line_size(int index) const;
        int get_text_lines_size_up_to_index(int index) const;
        std::string get_text() const;
        ftxui::Elements get_text_lines() const;
    private:
        std::string text_;
        ftxui::Elements text_lines_;
        std::vector<int> text_lines_size_;

        void populate_text_lines();
        void push_line(ftxui::Elements& line, int& num_of_characters);
};


#endif //TEXT_H
