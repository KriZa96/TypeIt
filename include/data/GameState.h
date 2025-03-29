//
// Created by kil3 on 3/15/25.
//

#ifndef GAMESTATE_H
#define GAMESTATE_H

#include <filesystem>
#include <fstream>
#include "GameOptions.h"
#include "../core/FileTextSource.h"

struct GameState {
    inline static bool game_session_in_progress = false;
    inline static bool start_session = false;
    inline static bool refresh_session = false;
    inline static bool game_finished = true;
    inline static bool show_info = false;

    static void toggle_info_display() {
        show_info = !show_info;
    }

    static void start_game_session() {
        const std::string& path = GameOptions::text_radiobox_values_[GameOptions::selected_radiobox_text_];
        const int time = GameOptions::time_radiobox_values_[GameOptions::selected_radiobox_time_];
        if (FileTextSource::is_file_valid(path) && time > 0) {
            start_session = true;
        }
    }

};


#endif //GAMESTATE_H
