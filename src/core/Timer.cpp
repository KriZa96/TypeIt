//
// Created by kil3 on 3/1/25.
//
#include "../../include/core/Timer.h"

#include <format>
#include <mutex>

#include "../../include/data/GameState.h"


Timer::Timer(const int total_time):
            start_time_(std::chrono::steady_clock::now()),
            total_time_(total_time),
            elapsed_time_(0),
            started_timer_(false)
{}


int Timer::get_elapsed_time() {
    if (not GameState::game_session_in_progress_) {
        return 0;
    }
    if (not started_timer_) {
        started_timer_ = true;
        start_time_ = std::chrono::steady_clock::now();
    }
    if (elapsed_time_ >= total_time_) {
        GameState::game_finished_ = true;
        return total_time_;
    }
    elapsed_time_ = static_cast<int>(std::chrono::duration_cast<std::chrono::seconds>(
       std::chrono::steady_clock::now() - start_time_
    ).count());
    return elapsed_time_;
}


std::string Timer::get_time_left_str() {
    return std::format("{}s", std::max(0, total_time_ - get_elapsed_time()));
}


ftxui::Element Timer::get_time_element() {
    return ftxui::text(get_time_left_str());
}


ftxui::Component Timer::get_time_component() {
    return ftxui::Renderer(
        [&] {
            return get_time_element();
        }
    );
}


int& Timer::get_elapsed_time_reference() {
    return elapsed_time_;
}
