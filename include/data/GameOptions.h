//
// Created by kil3 on 3/13/25.
//

#ifndef GAMEOPTIONS_H
#define GAMEOPTIONS_H

#include <string>
#include <vector>

struct GameOptions {
    inline static std::vector<int> time_radiobox_values_{15, 30, 60};
    inline static int selected_radiobox_time_ = 0;

    inline static std::vector<std::string> text_radiobox_values_{
        "../../files/simple.txt", "../../files/medium.txt", "../../files/hard.txt", ""
    };
    inline static int selected_radiobox_text_ = 0;
};

#endif //GAMEOPTIONS_H
