// hotd3-re — The House of the Dead III: Reverse Engineered
// SPDX-License-Identifier: GPL-3.0-or-later
//
// RIFF WAVE (.wav / .aif) PCM audio loader

#pragma once

#include "core/types.h"
#include <vector>
#include <string>
#include <filesystem>

namespace hotd3::audio {

struct WavAudioData {
    u32 sample_rate = 22050;
    u16 channels = 1;
    u16 bits_per_sample = 16;
    std::vector<s16> pcm_samples;
    bool is_valid = false;
};

class WavLoader {
public:
    static WavAudioData load_from_memory(const u8* data, size_t size);
    static WavAudioData load_from_file(const std::filesystem::path& path);
};

} // namespace hotd3::audio
