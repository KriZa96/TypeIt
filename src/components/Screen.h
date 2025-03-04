//
// Created by kil3 on 3/4/25.
//

#ifndef SCREEN_H
#define SCREEN_H
#include <chrono>
#include <thread>
#include "ftxui/component/component.hpp"
#include "ftxui/component/screen_interactive.hpp"


class Screen {
    public:
        Screen():
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

        ~Screen() {
            running_ = false;
            if (screen_thread_.joinable()) {
                screen_thread_.join();
            }
        }

        void loop(const ftxui::Component& component) {
            screen_.Loop(component);
        }

    private:
        ftxui::ScreenInteractive screen_;
        std::atomic<bool> running_;
        std::thread screen_thread_;
};



#endif //SCREEN_H
