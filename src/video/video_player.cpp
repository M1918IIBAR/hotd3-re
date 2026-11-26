// hotd3-re — The House of the Dead III: Reverse Engineered
// SPDX-License-Identifier: GPL-3.0-or-later

#include "video/video_player.h"
#include "render/renderer.h"
#include <cstdio>
#include <filesystem>

namespace hotd3 {

VideoPlayer::VideoPlayer() = default;

VideoPlayer::~VideoPlayer() {
    shutdown();
}

bool VideoPlayer::init() {
    std::printf("[Video] Initializing video player...\n");
    initialized_ = true;
    return true;
}

void VideoPlayer::shutdown() {
    stop();
    initialized_ = false;
}

bool VideoPlayer::open_file(const std::string& path) {
    if (!std::filesystem::exists(path)) {
        std::fprintf(stderr, "[Video] File not found: %s\n", path.c_str());
        return false;
    }

    current_video_path_ = path;
    current_playback_time_ = 0.0;
    has_finished_ = false;
    std::printf("[Video] Opened video file: %s\n", path.c_str());
    return true;
}

void VideoPlayer::play() {
    if (current_video_path_.empty()) return;
    is_playing_ = true;
    has_finished_ = false;
}

void VideoPlayer::pause() {
    is_playing_ = false;
}

void VideoPlayer::stop() {
    is_playing_ = false;
    current_playback_time_ = 0.0;
    current_video_path_.clear();
}

void VideoPlayer::update(f64 dt) {
    if (!is_playing_) return;

    current_playback_time_ += dt;
    // When playback completes, trigger has_finished_
}

void VideoPlayer::render(IRenderer* renderer) {
    if (!renderer || !is_playing_) return;
    // Draw current decoded video frame to fullscreen quad
}

} // namespace hotd3
