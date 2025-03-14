//
// Created by kil3 on 3/14/25.
//

#ifndef MAIN_H
#define MAIN_H

#include "ftxui/component/component.hpp"

#include "./Screen.h"
#include "./Menu.h"
#include "./SpeedTypingSession.h"
#include "./GameOptions.h"

class Main {
    public:
        void start_game() {
            ftxui::Component main_component = get_main_component();
            screen.loop(main_component);
        }

        void refresh_component() {
            speed_typing_session_ = std::make_shared<SpeedTypingSession>(
                GameOptions::time_radiobox_values_[GameOptions::selected_radiobox_time_],
                "Amidst the labyrinthine corridors of the antiquated library, an astute scholar meticulously perused a compendium of esoteric manuscripts. The air was imbued with the musty aroma of aged parchment, mingling with the subdued flicker of incandescent sconces affixed to mahogany walls. Each tome contained a plethora of arcane knowledge, its intricacies demanding unwavering concentration. A colossal grandfather clock punctuated the silence, its pendulum oscillating with measured precision. Beyond the latticed windows, a tempestuous gale howled through the desolate alleyways, sending spirals of detritus skittering across the cobblestone thoroughfare. The scholar, undeterred by the cacophonous symphony of the storm, continued deciphering the convoluted script, fingers tracing the ornate calligraphy etched in gilded ink."
            );
            speed_typing_session_component_ = speed_typing_session_->get_speed_typing_session_component();
            container_ = ftxui::Container::Stacked(
            {
                menu_component_,
                speed_typing_session_component_
            }
        );
        }

        ftxui::Component get_main_component() {
            return ftxui::Renderer(
                container_,
                [this] {
                    if (GameOptions::start_game_ && !GameOptions::game_started_) {
                        speed_typing_session_ = std::make_shared<SpeedTypingSession>(
                            GameOptions::time_radiobox_values_[GameOptions::selected_radiobox_time_],
                            "Amidst the labyrinthine corridors of the antiquated library, an astute scholar meticulously perused a compendium of esoteric manuscripts. The air was imbued with the musty aroma of aged parchment, mingling with the subdued flicker of incandescent sconces affixed to mahogany walls. Each tome contained a plethora of arcane knowledge, its intricacies demanding unwavering concentration. A colossal grandfather clock punctuated the silence, its pendulum oscillating with measured precision. Beyond the latticed windows, a tempestuous gale howled through the desolate alleyways, sending spirals of detritus skittering across the cobblestone thoroughfare. The scholar, undeterred by the cacophonous symphony of the storm, continued deciphering the convoluted script, fingers tracing the ornate calligraphy etched in gilded ink."
                        );
                        speed_typing_session_component_ = speed_typing_session_->get_speed_typing_session_component();
                        GameOptions::game_started_ = true;
                        GameOptions::start_game_ = false;
                        return speed_typing_session_component_->Render();
                    }
                    if (GameOptions::game_started_) {
                        return speed_typing_session_component_->Render();
                    }
                    speed_typing_session_ = nullptr;
                    return menu_component_->Render();
                }
            );
        }

    private:
        Screen screen;
        Menu menu_;
        std::shared_ptr<SpeedTypingSession> speed_typing_session_ = std::make_shared<SpeedTypingSession>(
            30,
            "Amidst the labyrinthine corridors of the antiquated library, an astute scholar meticulously perused a compendium of esoteric manuscripts. The air was imbued with the musty aroma of aged parchment, mingling with the subdued flicker of incandescent sconces affixed to mahogany walls. Each tome contained a plethora of arcane knowledge, its intricacies demanding unwavering concentration. A colossal grandfather clock punctuated the silence, its pendulum oscillating with measured precision. Beyond the latticed windows, a tempestuous gale howled through the desolate alleyways, sending spirals of detritus skittering across the cobblestone thoroughfare. The scholar, undeterred by the cacophonous symphony of the storm, continued deciphering the convoluted script, fingers tracing the ornate calligraphy etched in gilded ink."
            );

        ftxui::Component speed_typing_session_component_ = speed_typing_session_->get_speed_typing_session_component();

        ftxui::Component menu_component_ = menu_.get_menu_component();

        ftxui::Component container_ = ftxui::Container::Stacked(
            {
                menu_component_,
                speed_typing_session_component_
            }
        );
};



#endif //MAIN_H
