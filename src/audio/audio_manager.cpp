// hotd3-re — The House of the Dead III: Reverse Engineered
// SPDX-License-Identifier: GPL-3.0-or-later

#include "audio/audio_manager.h"
#include <cstdio>
#include <algorithm>

namespace hotd3 {

AudioManager::AudioManager() = default;

AudioManager::~AudioManager() {
    shutdown();
}

bool AudioManager::init() {
    std::printf("[Audio] Initializing audio subsystem...\n");
    initialized_ = true;
    return true;
}

void AudioManager::shutdown() {
    if (!initialized_) return;
    stop_bgm();
    std::printf("[Audio] Audio subsystem shutdown complete\n");
    initialized_ = false;
}

void AudioManager::update(f64 /*dt*/) {
    if (!initialized_) return;
}

void AudioManager::set_paths(const std::filesystem::path& game_data, const std::filesystem::path& extracted) {
    game_data_path_ = game_data;
    extracted_path_ = extracted;
}

void AudioManager::play_bgm(const std::string& track_name, bool loop) {
    if (!initialized_) return;
    current_bgm_track_ = track_name;
    bgm_playing_ = true;

    // Check cache or load ADX stream
    if (bgm_cache_.find(track_name) == bgm_cache_.end()) {
        auto bgm_file = extracted_path_ / "BGM" / (track_name + ".adx");
        if (std::filesystem::exists(bgm_file)) {
            auto decoded = audio::AdxDecoder::decode_from_file(bgm_file);
            if (decoded.is_valid) {
                std::printf("[Audio] Loaded BGM: %s (%u Hz, %u ch, %u samples)\n",
                            track_name.c_str(), decoded.sample_rate, decoded.channels, decoded.total_samples);
                bgm_cache_[track_name] = std::move(decoded);
            }
        }
    }

    std::printf("[Audio] Playing BGM: %s (loop=%s)\n", track_name.c_str(), loop ? "true" : "false");
}

void AudioManager::pause_bgm() {
    if (!initialized_) return;
    bgm_playing_ = false;
    std::printf("[Audio] Paused BGM\n");
}

void AudioManager::resume_bgm() {
    if (!initialized_) return;
    bgm_playing_ = true;
    std::printf("[Audio] Resumed BGM\n");
}

void AudioManager::stop_bgm() {
    if (!initialized_) return;
    bgm_playing_ = false;
    current_bgm_track_.clear();
    std::printf("[Audio] Stopped BGM\n");
}

void AudioManager::play_sfx(const std::string& sound_name, f32 volume) {
    if (!initialized_) return;

    if (sfx_cache_.find(sound_name) == sfx_cache_.end()) {
        auto sfx_path = game_data_path_ / "media" / (sound_name + ".aif");
        if (std::filesystem::exists(sfx_path)) {
            auto wav = audio::WavLoader::load_from_file(sfx_path);
            if (wav.is_valid) {
                sfx_cache_[sound_name] = std::move(wav);
            }
        }
    }

    std::printf("[Audio] Play SFX: %s (vol=%.2f)\n", sound_name.c_str(), volume * sfx_volume_ * master_volume_);
}

void AudioManager::play_voice(const std::string& voice_name, f32 volume) {
    if (!initialized_) return;

    auto voice_file = extracted_path_ / "VOICE_US" / (voice_name + ".adx");
    if (std::filesystem::exists(voice_file)) {
        std::printf("[Audio] Play Voice: %s (vol=%.2f)\n", voice_name.c_str(), volume * voice_volume_ * master_volume_);
    }
}

void AudioManager::set_master_volume(f32 volume) {
    master_volume_ = std::clamp(volume, 0.0f, 1.0f);
}

void AudioManager::set_bgm_volume(f32 volume) {
    bgm_volume_ = std::clamp(volume, 0.0f, 1.0f);
}

void AudioManager::set_sfx_volume(f32 volume) {
    sfx_volume_ = std::clamp(volume, 0.0f, 1.0f);
}

void AudioManager::set_voice_volume(f32 volume) {
    voice_volume_ = std::clamp(volume, 0.0f, 1.0f);
}

} // namespace hotd3
