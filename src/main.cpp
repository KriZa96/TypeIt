//
// Created by kil3 on 2/15/25.
//
#include "../include/Timer.h"
#include "../include/Screen.h"

int main() {
    Screen screen{};
    Timer time{10};

    auto renderer = ftxui::Renderer(
        [&] {
            return time.get_time_element();
        }
    );

    screen.loop(renderer);
  return 0;
}