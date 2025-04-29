//
// Created by Kristijan Zalac on 3/13/25.
//

#ifndef GAMEOPTIONS_H
#define GAMEOPTIONS_H

#include <filesystem>
#include <string>
#include <vector>

struct GameOptions {
    inline static std::vector<int> time_radiobox_values_{15, 30, 60, 0};
    inline static int selected_radiobox_time_ = 0;
    inline static std::string custom_radiobox_input_;

    inline static std::filesystem::path current_file_path = __FILE__;
    inline static std::filesystem::path target_file_path =
            current_file_path.parent_path().parent_path().parent_path() / "files";
    inline static std::vector<std::string> text_radiobox_values_{
            (target_file_path / "simple.txt").string(),
            (target_file_path / "medium.txt").string(),
            (target_file_path / "hard.txt").string(),
            ""
    };
    inline static int selected_radiobox_text_ = 0;
};

#endif //GAMEOPTIONS_H
