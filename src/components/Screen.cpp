//
// Created by kil3 on 3/4/25.
//

#include "Screen.h"

Screen::Screen():
            screen_(ftxui::ScreenInteractive::Fullscreen()),
            running_(true)
{
    screen_thread_ = std::thread(
        [this] {
            while (running_) {
                screen_.PostEvent(ftxui::Event::Custom);
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
            }
        }
    );
}

Screen::~Screen() {
    running_ = false;
    if (screen_thread_.joinable()) {
        screen_thread_.join();
    }
}

void Screen::loop(const ftxui::Component& component) {
    screen_.Loop(component);
}

