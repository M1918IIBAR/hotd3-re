// hotd3-re — The House of the Dead III: Reverse Engineered
// SPDX-License-Identifier: GPL-3.0-or-later

#include "core/timer.h"

namespace hotd3 {

Timer::Timer() {
    reset();
}

void Timer::reset() {
    start_time_      = Clock::now();
    prev_time_       = start_time_;
    delta_time_      = 0.0;
    total_time_      = 0.0;
    frame_count_     = 0;
    fps_timer_       = 0.0;
    fps_frame_count_ = 0;
    cached_fps_      = 0.0;
}

void Timer::tick() {
    auto now = Clock::now();
    delta_time_ = std::chrono::duration<f64>(now - prev_time_).count();
    total_time_ = std::chrono::duration<f64>(now - start_time_).count();
    prev_time_  = now;
    frame_count_++;

    // Update FPS counter (every second)
    fps_timer_ += delta_time_;
    fps_frame_count_++;
    if (fps_timer_ >= 1.0) {
        cached_fps_ = static_cast<f64>(fps_frame_count_) / fps_timer_;
        fps_timer_ = 0.0;
        fps_frame_count_ = 0;
    }
}

f64 Timer::fps() const {
    return cached_fps_;
}

} // namespace hotd3
