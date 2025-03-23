//
// Created by kil3 on 3/15/25.
//

#ifndef GAMESTATE_H
#define GAMESTATE_H

#include <filesystem>
#include <fstream>
#include "GameOptions.h"

struct GameState {
    inline static bool game_session_in_progress = false;
    inline static bool refresh_session = false;
    inline static bool game_finished = true;
    inline static bool show_info = false;

    static void toggle_info_display() {
        show_info = !show_info;
    }

    static bool is_file_valid(const std::string& path) {
        if (std::filesystem::is_directory(path)) {
            return false;
        }
        const std::ifstream file_stream(path);
        return file_stream ? true : false;
    }
    static bool is_time_valid(const int time) {
        return time > 0;
    }
    static void start_game_session() {
        const std::string& path = GameOptions::text_radiobox_values_[GameOptions::selected_radiobox_text_];
        const int time = GameOptions::time_radiobox_values_[GameOptions::selected_radiobox_time_];
        if (is_file_valid(path) && is_time_valid(time)) {
            refresh_session = true;
        }
    }

};


#endif //GAMESTATE_H
