//
// Created by kriza on 31/03/2025.
//

#ifndef TYPEIT_INPUTACCURACYENGINE_H
#define TYPEIT_INPUTACCURACYENGINE_H

#include <vector>
#include "ftxui/component/component.hpp"


class InputAccuracyEngine {
public:
    void push_character_accuracy(bool value);
    [[nodiscard]] float get_percentage_of_correct_input() const;
private:
    std::vector<bool> character_accuracy_;
};


#endif  // TYPEIT_INPUTACCURACYENGINE_H
