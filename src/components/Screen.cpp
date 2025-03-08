//
// Created by kil3 on 3/4/25.
//

#include "../../include/Screen.h"


Screen::Screen():
            screen_(ftxui::ScreenInteractive::Fullscreen())
{
    start_refresh();
    screen_thread_ = std::thread(
        [this] {
            while (refresh_) {
                screen_.PostEvent(ftxui::Event::Custom);
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
            }
        }
    );
}


Screen::~Screen() {
    stop_refresh();
    if (screen_thread_.joinable()) {
        screen_thread_.join();
    }
}


void Screen::loop(const ftxui::Component& component) {
    screen_.Loop(component);
}


void Screen::start_refresh() {
    refresh_ = true;
}


void Screen::stop_refresh() {
    refresh_ = false;
}

