// hotd3-re — The House of the Dead III: Reverse Engineered
// SPDX-License-Identifier: GPL-3.0-or-later
//
// AIFF / AIFF-C audio loader.
//
// HOTD3 PC stores sound effects in Apple AIFF format (big-endian):
//   assets/game_data/media/*.aif
//
// Container layout:
//   FORM  <size:u32be>  "AIFF"
//     COMM  <size:u32be>  <channels:u16be> <frames:u32be> <bitdepth:u16be>
//                         <samplerate:80-bit IEEE-754 extended>
//     SSND  <size:u32be>  <offset:u32be> <block:u32be>  <pcm-data>

#pragma once

#include "core/types.h"
#include <vector>
#include <filesystem>

namespace hotd3::audio {

struct AiffAudioData {
    bool is_valid = false;

    u32 sample_rate   = 0;
    u16 channels      = 0;
    u16 bits_per_sample = 0;
    u32 frame_count   = 0;         // number of sample frames

    std::vector<u8> pcm_data;      // interleaved, big-endian 16-bit samples
                                   // (converted to host endian on load)
};

class AiffLoader {
public:
    // Load AIFF from an in-memory buffer.
    static AiffAudioData load_from_memory(const u8* data, size_t size);

    // Load AIFF from a file path.
    static AiffAudioData load_from_file(const std::filesystem::path& path);

private:
    // Convert 80-bit IEEE 754 extended precision (big-endian) → double.
    static double extended_to_double(const u8 bytes[10]);
};

} // namespace hotd3::audio
