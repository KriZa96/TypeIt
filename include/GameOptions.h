//
// Created by kil3 on 3/13/25.
//

#ifndef GAMEOPTIONS_H
#define GAMEOPTIONS_H

#include <string>

struct GameOptions {
    inline static std::vector<int> time_radiobox_values_{15, 30, 60};
    inline static int selected_radiobox_time_ = 0;

    inline static std::string custom_file_path_;
    inline static std::vector<std::string> text_radiobox_values_{
        "../../files/simple.txt", "../../files/medium.txt", "../../files/hard.txt", custom_file_path_
    };
    inline static int selected_radiobox_text_ = 0;

    inline static bool start_game_ = false;
    inline static bool game_started_ = false;
};

#endif //GAMEOPTIONS_H
