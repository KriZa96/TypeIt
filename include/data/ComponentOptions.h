//
// Created by kil3 on 3/23/25.
//

#ifndef COMPONENTOPTIONS_H
#define COMPONENTOPTIONS_H

#include "ftxui/component/component.hpp"

#include "Style.h"

struct ComponentOptions {
    static inline ftxui::RadioboxOption menu_radiobox_option = {
        .transform = [](const ftxui::EntryState& state) {
        #if defined(FTXUI_MICROSOFT_TERMINAL_FALLBACK)
            auto prefix = ftxui::text(state.state ? "(*) " : "( ) ") | ftxui::color(ftxui::Color::Grey85);
        #else
            auto prefix = ftxui::text(state.state ? "◉ " : "○ ") | Style::input_text_color_good;
        #endif
            auto t = ftxui::text(state.label) | Style::input_text_color_good;
            if (state.focused) {
                t |= ftxui::inverted;
            }
            return ftxui::hbox({prefix, t});
        }
    };

    static inline ftxui::InputOption menu_path_input_option = {.transform = [](ftxui::InputState state) {
        state.element |= ftxui::border;

        if (state.hovered || state.focused) {
            const std::string& path = GameOptions::text_radiobox_values_[GameOptions::selected_radiobox_text_];
            if (GameState::is_file_valid(path)) {
                state.element |= Style::input_text_color_good;
            }
            else {
                state.element |= Style::input_text_color_bad;
            }
        }
        else {
            state.element |= Style::underlying_text_color;
        }

        return state.element;
    }};

    static inline ftxui::InputOption menu_time_input_option = {.transform = [](ftxui::InputState state) {
        try {
            GameOptions::time_radiobox_values_[3] = std::stoi(GameOptions::custom_radiobox_input_);
        } catch (const std::invalid_argument& _) {}

        state.element |= ftxui::border;

        if (state.hovered || state.focused) {
            const int time = GameOptions::time_radiobox_values_[GameOptions::selected_radiobox_time_];
            if (GameState::is_time_valid(time)) {
                state.element |= Style::input_text_color_good;
            }
            else {
                state.element |= Style::input_text_color_bad;
            }
        }
        else {
            state.element |= Style::underlying_text_color;
        }

        return state.element;
    }};

};

#endif //COMPONENTOPTIONS_H
