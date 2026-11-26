// hotd3-re — The House of the Dead III: Reverse Engineered
// SPDX-License-Identifier: GPL-3.0-or-later
//
// Sega Sofdec (.sfd) FMV cutscene video player

#pragma once

#include "core/types.h"
#include <string>

namespace hotd3 {

class IRenderer;

class VideoPlayer {
public:
    VideoPlayer();
    ~VideoPlayer();

    bool init();
    void shutdown();

    bool open_file(const std::string& path);
    void play();
    void pause();
    void stop();

    bool is_playing() const { return is_playing_; }
    bool has_finished() const { return has_finished_; }

    void update(f64 dt);
    void render(IRenderer* renderer);

private:
    bool initialized_ = false;
    bool is_playing_ = false;
    bool has_finished_ = false;

    std::string current_video_path_;
    f64 current_playback_time_ = 0.0;
    f64 video_duration_ = 0.0;

    u32 video_width_ = 640;
    u32 video_height_ = 480;
};

} // namespace hotd3
