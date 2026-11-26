// hotd3-re — The House of the Dead III: Reverse Engineered
// SPDX-License-Identifier: GPL-3.0-or-later

#include "audio/adx_decoder.h"
#include <fstream>
#include <cmath>
#include <iostream>
#include <algorithm>

namespace hotd3::audio {

static inline u16 read_be16(const u8* p) {
    return static_cast<u16>((p[0] << 8) | p[1]);
}

static inline u32 read_be32(const u8* p) {
    return (static_cast<u32>(p[0]) << 24) |
           (static_cast<u32>(p[1]) << 16) |
           (static_cast<u32>(p[2]) << 8)  |
           (static_cast<u32>(p[3]));
}

AdxAudioData AdxDecoder::decode_from_memory(const u8* data, size_t size) {
    AdxAudioData audio{};
    if (!data || size < 24) {
        std::cerr << "[AdxDecoder] Buffer too small for ADX header\n";
        return audio;
    }

    if (data[0] != 0x80 || data[1] != 0x00) {
        std::cerr << "[AdxDecoder] Invalid ADX signature\n";
        return audio;
    }

    u16 copyright_offset = read_be16(data + 2);
    u8 encoding_type = data[4];
    u8 block_size = data[5];
    u8 sample_bitdepth = data[6];
    u8 channels = data[7];
    u32 sample_rate = read_be32(data + 8);
    u32 total_samples = read_be32(data + 12);
    u16 highpass_freq = read_be16(data + 16);

    if (channels == 0 || channels > 2 || block_size != 18 || sample_bitdepth != 4) {
        std::cerr << "[AdxDecoder] Unsupported ADX format (ch=" << (int)channels
                  << " blk=" << (int)block_size << " bits=" << (int)sample_bitdepth << ")\n";
        return audio;
    }

    audio.sample_rate = sample_rate;
    audio.channels = channels;
    audio.total_samples = total_samples;

    // Check loop info if header is large enough
    if (copyright_offset >= 32) {
        u16 loop_flag = read_be16(data + 24);
        if (loop_flag == 1) {
            audio.has_loop = true;
            audio.loop_start_sample = read_be32(data + 28);
            audio.loop_end_sample = read_be32(data + 32);
        }
    }

    // Calculate prediction coefficients
    f64 freq = (highpass_freq > 0) ? highpass_freq : 500.0;
    f64 a = std::sqrt(2.0) - std::cos(2.0 * 3.14159265358979323846 * freq / sample_rate);
    f64 b = std::sqrt(2.0) - 1.0;
    f64 c = (a - std::sqrt((a + b) * (a - b))) / b;
    f64 coef1 = 2.0 * c;
    f64 coef2 = -(c * c);

    // Audio payload starts at copyright_offset + 4
    size_t data_offset = copyright_offset + 4;
    if (data_offset >= size) {
        std::cerr << "[AdxDecoder] Data offset exceeds file size\n";
        return audio;
    }

    audio.pcm_samples.resize(total_samples * channels, 0);

    // Per-channel prediction history
    f64 hist1[2] = { 0.0, 0.0 };
    f64 hist2[2] = { 0.0, 0.0 };

    size_t samples_decoded = 0;
    const u8* cur = data + data_offset;
    const u8* end = data + size;

    constexpr u32 SAMPLES_PER_BLOCK = 32;

    while (cur + (block_size * channels) <= end && samples_decoded < total_samples) {
        // Read each channel's block
        for (u8 ch = 0; ch < channels; ++ch) {
            const u8* block = cur + (ch * block_size);
            u16 scale = read_be16(block);

            for (u32 s = 0; s < SAMPLES_PER_BLOCK && (samples_decoded + s) < total_samples; ++s) {
                u8 byte_val = block[2 + (s / 2)];
                s8 nibble = (s % 2 == 0) ? static_cast<s8>((byte_val >> 4) & 0x0F)
                                         : static_cast<s8>(byte_val & 0x0F);
                // Sign extend 4-bit to 8-bit signed
                if (nibble & 0x08) {
                    nibble |= 0xF0;
                }

                f64 sample = (nibble * scale) + (coef1 * hist1[ch]) + (coef2 * hist2[ch]);
                hist2[ch] = hist1[ch];
                hist1[ch] = sample;

                // Clamp to 16-bit signed
                s32 clamped = static_cast<s32>(std::round(sample));
                clamped = std::clamp(clamped, -32768, 32767);

                audio.pcm_samples[(samples_decoded + s) * channels + ch] = static_cast<s16>(clamped);
            }
        }

        cur += block_size * channels;
        samples_decoded += SAMPLES_PER_BLOCK;
    }

    audio.is_valid = true;
    return audio;
}

AdxAudioData AdxDecoder::decode_from_file(const std::filesystem::path& path) {
    std::ifstream file(path, std::ios::binary | std::ios::ate);
    if (!file.is_open()) {
        std::cerr << "[AdxDecoder] Failed to open: " << path << "\n";
        return {};
    }

    auto file_size = static_cast<size_t>(file.tellg());
    file.seekg(0, std::ios::beg);

    std::vector<u8> buffer(file_size);
    file.read(reinterpret_cast<char*>(buffer.data()), file_size);

    return decode_from_memory(buffer.data(), file_size);
}

} // namespace hotd3::audio
