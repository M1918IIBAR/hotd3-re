// hotd3-re — The House of the Dead III: Reverse Engineered
// SPDX-License-Identifier: GPL-3.0-or-later
//
// High-resolution timer for frame timing and game logic.

#pragma once

#include "core/types.h"
#include <chrono>

namespace hotd3 {

class Timer {
public:
    Timer();

    void  reset();
    void  tick();

    f64   delta_time() const { return delta_time_; }   // Seconds since last tick
    f64   total_time() const { return total_time_; }   // Seconds since reset
    u64   frame_count() const { return frame_count_; }
    f64   fps() const;

    // Fixed timestep support (for arcade-accurate 60fps logic)
    static constexpr f64 FIXED_TIMESTEP = 1.0 / 60.0;  // 16.667ms

private:
    using Clock     = std::chrono::high_resolution_clock;
    using TimePoint = Clock::time_point;

    TimePoint start_time_;
    TimePoint prev_time_;
    f64       delta_time_ = 0.0;
    f64       total_time_ = 0.0;
    u64       frame_count_ = 0;

    // FPS calculation
    f64       fps_timer_ = 0.0;
    u32       fps_frame_count_ = 0;
    f64       cached_fps_ = 0.0;
};

} // namespace hotd3
