// hotd3-re — The House of the Dead III: Reverse Engineered
// SPDX-License-Identifier: GPL-3.0-or-later
//
// CRI ADX audio decoder — decodes ADX compressed streams to 16-bit PCM

#pragma once

#include "core/types.h"
#include <vector>
#include <string>
#include <filesystem>

namespace hotd3::audio {

struct AdxAudioData {
    u32 sample_rate = 48000;
    u16 channels = 2;
    u32 total_samples = 0;
    bool has_loop = false;
    u32 loop_start_sample = 0;
    u32 loop_end_sample = 0;
    std::vector<s16> pcm_samples; // Interleaved 16-bit signed PCM
    bool is_valid = false;
};

class AdxDecoder {
public:
    static AdxAudioData decode_from_memory(const u8* data, size_t size);
    static AdxAudioData decode_from_file(const std::filesystem::path& path);
};

} // namespace hotd3::audio
