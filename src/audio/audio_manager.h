// hotd3-re — The House of the Dead III: Reverse Engineered
// SPDX-License-Identifier: GPL-3.0-or-later
//
// Audio subsystem interface and manager

#pragma once

#include "core/types.h"
#include "audio/adx_decoder.h"
#include "audio/wav_loader.h"
#include "audio/aiff_loader.h"

#include <string>
#include <memory>
#include <unordered_map>
#include <filesystem>

namespace hotd3 {

class AudioManager {
public:
    AudioManager();
    ~AudioManager();

    bool init();
    void shutdown();

    void set_paths(const std::filesystem::path& game_data, const std::filesystem::path& extracted);

    void update(f64 dt);

    // Music playback (CRI ADX streams from BGM.AFS)
    void play_bgm(const std::string& track_name, bool loop = true);
    void pause_bgm();
    void resume_bgm();
    void stop_bgm();

    // Sound effects (media/*.aif — Apple AIFF format)
    void play_sfx(const std::string& sound_name, f32 volume = 1.0f);

    // Voice lines (VOICE_US.AFS)
    void play_voice(const std::string& voice_name, f32 volume = 1.0f);

    // Volume controls [0.0 - 1.0]
    void set_master_volume(f32 volume);
    void set_bgm_volume(f32 volume);
    void set_sfx_volume(f32 volume);
    void set_voice_volume(f32 volume);

    f32 master_volume() const { return master_volume_; }
    f32 bgm_volume() const { return bgm_volume_; }
    f32 sfx_volume() const { return sfx_volume_; }
    f32 voice_volume() const { return voice_volume_; }

private:
    bool initialized_ = false;
    f32 master_volume_ = 1.0f;
    f32 bgm_volume_ = 0.8f;
    f32 sfx_volume_ = 1.0f;
    f32 voice_volume_ = 1.0f;

    std::filesystem::path game_data_path_;
    std::filesystem::path extracted_path_;

    std::string current_bgm_track_;
    bool bgm_playing_ = false;

    // Audio caches
    std::unordered_map<std::string, audio::AiffAudioData> sfx_cache_;
    std::unordered_map<std::string, audio::AdxAudioData>  bgm_cache_;
};

} // namespace hotd3
