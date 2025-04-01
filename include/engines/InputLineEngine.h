//
// Created by kriza on 01/04/2025.
//

#ifndef TYPEIT_INPUTLINEENGINE_H
#define TYPEIT_INPUTLINEENGINE_H

#include "../core/Text.h"
#include "../engines/InputAccuracyEngine.h"


class InputLineEngine {
public:
    explicit InputLineEngine(std::shared_ptr<Text> text_instance);
    void render_input_text(char next_character, std::size_t input_text_size);
    [[nodiscard]] float get_percentage_of_correct_input() const;
    [[nodiscard]] ftxui::Elements get_total_input_lines() const;
    [[nodiscard]] std::size_t get_current_line_index() const;
    [[nodiscard]] std::size_t get_current_line_size() const;
private:
    std::size_t current_line_index_;
    InputAccuracyEngine input_accuracy_;
    std::shared_ptr<Text> text_instance_;
    ftxui::Elements current_input_line_;
    ftxui::Elements total_input_lines_;
    std::vector<ftxui::Elements> previous_input_lines_;

    void go_to_new_line();
    void go_to_previous_line();
    void remove_element(std::size_t input_text_size);
    void add_element(char last_letter);
    bool should_finish_game();
    [[nodiscard]] int get_previous_lines_size() const;
    [[nodiscard]] ftxui::Element get_next_character(char next_character);
    [[nodiscard]] bool should_add_element(std::size_t input_text_size) const;
    [[nodiscard]] bool should_remove_element(std::size_t input_text_size) const;
    [[nodiscard]] bool should_go_to_next_line(char next_character) const;
};


#endif  // TYPEIT_INPUTLINEENGINE_H
