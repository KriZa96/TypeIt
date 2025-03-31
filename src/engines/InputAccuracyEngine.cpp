//
// Created by kriza on 31/03/2025.
//

#include "../../include/engines/InputAccuracyEngine.h"


void InputAccuracyEngine::push_character_accuracy(bool value){
    character_accuracy_.push_back(value);
}


[[nodiscard]] float InputAccuracyEngine::get_percentage_of_correct_input() const {
    const float character_accuracy_size = character_accuracy_.size();
    if (character_accuracy_size <= 0) {
        return 0;
    }
    return 100.f * static_cast<float>(
   std::count(
       character_accuracy_.begin(),
       character_accuracy_.end(),
       true
   )) / character_accuracy_size;
}