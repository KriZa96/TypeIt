//
// Created by kil3 on 3/15/25.
//

#ifndef GAMESTATE_H
#define GAMESTATE_H

#include <filesystem>
#include <fstream>
#include "GameOptions.h"

struct GameState {
    inline static bool game_session_in_progress_ = false;
    inline static bool refresh_session_ = false;
    inline static bool game_finished_ = false;
    inline static bool show_info_ = false;

    static void toggle_game_session_display() {
        game_session_in_progress_ = !game_session_in_progress_;
    }
    static void toggle_refresh_session() {
        refresh_session_ = !refresh_session_;
    }
    static void toggle_game_finished() {
        game_finished_ = !game_finished_;
    }
    static void toggle_info_display() {
        show_info_ = !show_info_;
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
            toggle_refresh_session();
        }
    }

};


#endif //GAMESTATE_H
