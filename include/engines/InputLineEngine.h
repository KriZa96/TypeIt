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
    void render_input_text(const std::string& input_text);
    [[nodiscard]] float get_percentage_of_correct_input() const;
    [[nodiscard]] ftxui::Elements get_total_input_lines() const;
private:
    int current_line_index_;
    InputAccuracyEngine input_accuracy_;
    std::shared_ptr<Text> text_instance_;
    ftxui::Elements current_input_line_;
    ftxui::Elements total_input_lines_;
    std::vector<ftxui::Elements> previous_input_lines_;

    void go_to_new_line();
    void go_to_previous_line();
    void remove_element(const std::string& input_text);
    void add_element(char last_letter);
    bool should_finish_game();
    [[nodiscard]] int get_previous_lines_size() const;
    [[nodiscard]] ftxui::Element get_next_character(char last_letter);
    [[nodiscard]] bool should_add_element(std::size_t input_text_size) const;
    [[nodiscard]] bool should_remove_element(std::size_t input_text_size) const;
    [[nodiscard]] bool should_go_to_previous_line(const std::string& input_text) const;
    [[nodiscard]] bool should_go_to_next_line(char last_letter) const;
};


#endif  // TYPEIT_INPUTLINEENGINE_H
