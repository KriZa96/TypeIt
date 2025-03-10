//
// Created by kil3 on 2/15/25.
//
#include "../include/Screen.h"
#include "../include/TextInputArea.h"
#include "../include/Text.h"
#include "../include/SpeedTypingSession.h"


int main() {
    Screen screen;

    SpeedTypingSession speed_typing_session(30, "Amidst the labyrinthine corridors of the antiquated library, an astute scholar meticulously perused a compendium of esoteric manuscripts. The air was imbued with the musty aroma of aged parchment, mingling with the subdued flicker of incandescent sconces affixed to mahogany walls. Each tome contained a plethora of arcane knowledge, its intricacies demanding unwavering concentration. A colossal grandfather clock punctuated the silence, its pendulum oscillating with measured precision. Beyond the latticed windows, a tempestuous gale howled through the desolate alleyways, sending spirals of detritus skittering across the cobblestone thoroughfare. The scholar, undeterred by the cacophonous symphony of the storm, continued deciphering the convoluted script, fingers tracing the ornate calligraphy etched in gilded ink.");

    ftxui::Component component_ = speed_typing_session.get_speed_typing_session_component();

    screen.loop(component_);

    return 0;
}