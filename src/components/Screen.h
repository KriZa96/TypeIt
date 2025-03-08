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
        Screen();

        ~Screen();

        void loop(const ftxui::Component& component);

    private:
        ftxui::ScreenInteractive screen_;
        std::atomic<bool> running_;
        std::thread screen_thread_;
};



#endif //SCREEN_H
