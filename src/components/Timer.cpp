//
// Created by kil3 on 3/1/25.
//
#include "../../include/Timer.h"

#include <format>
#include <mutex>


Timer::Timer(const int total_time):
            start_time_(std::chrono::steady_clock::now()),
            total_time_(total_time),
            elapsed_time_(0) {}


int Timer::get_elapsed_time() {
    if (elapsed_time_ >= total_time_) {
        return total_time_;
    }
    elapsed_time_ = std::chrono::duration_cast<std::chrono::seconds>(
       std::chrono::steady_clock::now() - start_time_
    ).count();
    return elapsed_time_;
}


std::string Timer::get_time_left_str() {
    return std::format("{}s", std::max(0, total_time_ - get_elapsed_time()));
}


ftxui::Element Timer::get_time_element() {
    return ftxui::text(get_time_left_str());
}


std::shared_ptr<int> Timer::get_elapsed_time_shared_pointer() {
    return std::make_shared<int>(elapsed_time_);
}
