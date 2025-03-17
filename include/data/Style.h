//
// Created by kil3 on 3/16/25.
//

#ifndef STYLE_H
#define STYLE_H

#include "ftxui/component/component.hpp"

struct Style {

    inline static const ftxui::Decorator input_text_color_good = ftxui::color(ftxui::Color::Grey82);
    inline static const ftxui::Decorator input_text_color_bad = ftxui::color(ftxui::Color::Salmon1);
    inline static const ftxui::Decorator underlying_text_color = ftxui::color(ftxui::Color::Grey42);


    inline static const ftxui::FlexboxConfig full_center_config_{
        .justify_content = ftxui::FlexboxConfig::JustifyContent::Center,
        .align_items = ftxui::FlexboxConfig::AlignItems::Center,
        .align_content = ftxui::FlexboxConfig::AlignContent::Center
    };

    inline static const ftxui::FlexboxConfig space_around_config_{
        .justify_content = ftxui::FlexboxConfig::JustifyContent::SpaceAround,
        .align_items = ftxui::FlexboxConfig::AlignItems::Center,
        .align_content = ftxui::FlexboxConfig::AlignContent::Center
    };

    inline static const ftxui::FlexboxConfig space_between_config_{
        .justify_content = ftxui::FlexboxConfig::JustifyContent::SpaceBetween,
        .align_items = ftxui::FlexboxConfig::AlignItems::Center,
        .align_content = ftxui::FlexboxConfig::AlignContent::Center
    };

};

#endif //STYLE_H
