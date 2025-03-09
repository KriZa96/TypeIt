//
// Created by kil3 on 2/15/25.
//
#include "../include/Timer.h"
#include "../include/Screen.h"
#include <numeric>



int main() {
    /*Screen screen{};
    Timer time{10};

    auto renderer = ftxui::Renderer(
        [&] {
            return time.get_time_element();
        }
    );

    screen.loop(renderer);*/

    std::vector<int> some = {1, 1, 1};

    int sum = std::accumulate(
        some.begin(),
        some.begin() + 3,
        0
    );
    std::cout << sum << std::endl;
  return 0;
}