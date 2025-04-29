//
// Created by Kristijan Zalac on 3/23/25.
//

#ifndef COMPONENTOPTIONS_H
#define COMPONENTOPTIONS_H

#include "ftxui/component/component.hpp"

#include "Style.h"
#include "GameOptions.h"
#include "GameState.h"
#include "../core/FileTextSource.h"

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
            if (FileTextSource::is_file_valid(path)) {
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
            if (time > 0) {
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

    static inline ftxui::ComponentDecorator input_component_decorator = ftxui::CatchEvent([] (const ftxui::Event& event) {
        const std::string ctrl_t =  "\x14";
        if (event.character() == ctrl_t) {
            GameState::game_session_in_progress = false;
            return true;
        }

        const std::string ctrl_r = "\x12";
        if (event.character() == ctrl_r) {
            GameState::refresh_session = true;
            return true;
        }

        if (GameState::game_finished) {
            return true;
        }

        return false;
    });
};

#endif //COMPONENTOPTIONS_H
