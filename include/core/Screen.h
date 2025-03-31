//
// Created by kil3 on 3/4/25.
//

#ifndef SCREEN_H
#define SCREEN_H

#include <thread>

#include "gtest/gtest_prod.h"
#include "ftxui/component/component.hpp"
#include "ftxui/component/screen_interactive.hpp"


class Screen {
    public:
        Screen();
        ~Screen();
        void start_refresh();
        void stop_refresh();
        bool is_screen_refreshing() const;
        void loop(const ftxui::Component& component);
        ftxui::ScreenInteractive& get_screen_reference();
    private:
        ftxui::ScreenInteractive screen_;
        std::atomic<bool> refresh_;
        std::thread screen_thread_;
};



#endif //SCREEN_H
