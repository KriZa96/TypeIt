//
// Created by kil3 on 2/15/25.
//
#include "../include/Screen.h"
#include "../include/TextInputArea.h"
#include "../include/Menu.h"
#include "../include/SpeedTypingSession.h"
#include "../include/Main.h"


int main() {
    Screen screen;

    SpeedTypingSession speed_typing_session(30, "A curious rabbit explored the backyard, sniffing the gentle breeze that carried scents of fresh flowers and damp soil. The morning light filtered through the leaves, creating shifting patterns on the wooden fence. Birds chirped softly, their melodies weaving through the air like an unseen symphony. Nearby, a child balanced carefully on a stepping stone, arms stretched wide for stability. His laughter echoed as he leaped onto the grass, feeling the cool blades under his bare feet. The world felt calm yet alive, brimming with quiet energy. In the distance, the sound of a bicycle rolling down the street mixed with the occasional bark of a neighbor’s dog. A woman tended to her garden, trimming overgrown branches and watering the delicate petals of blooming roses. Time moved gently here, marked by the rhythm of daily life.");

    ftxui::Component component_ = speed_typing_session.get_speed_typing_session_component();

    Main main;
    ftxui::Component main_component = main.get_main_component();
    screen.loop(main_component);


    return 0;
}