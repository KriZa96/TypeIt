//
// Created by Kristijan Zalac on 3/4/25.
//

#include "../../include/core/Screen.h"


Screen::Screen():
screen_(ftxui::ScreenInteractive::Fullscreen())
{
    // Launch a background thread to periodically trigger UI refreshes.
    // This ensures real-time updates for the timer, WPM, and typing accuracy.
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


ftxui::ScreenInteractive& Screen::get_screen_reference() {
    return screen_;
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

bool Screen::is_screen_refreshing() const {
    return refresh_;
}

