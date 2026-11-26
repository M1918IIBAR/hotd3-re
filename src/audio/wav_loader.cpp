// hotd3-re — The House of the Dead III: Reverse Engineered
// SPDX-License-Identifier: GPL-3.0-or-later

#include "audio/wav_loader.h"
#include <fstream>
#include <cstring>
#include <iostream>

namespace hotd3::audio {

#pragma pack(push, 1)
struct RiffHeader {
    char riff[4];      // "RIFF"
    u32 file_size;
    char wave[4];      // "WAVE"
};

struct ChunkHeader {
    char id[4];
    u32 size;
};

struct FmtChunk {
    u16 format_tag;       // 1 = PCM
    u16 channels;         // 1 = mono, 2 = stereo
    u32 samples_per_sec;
    u32 avg_bytes_per_sec;
    u16 block_align;
    u16 bits_per_sample;
};
#pragma pack(pop)

WavAudioData WavLoader::load_from_memory(const u8* data, size_t size) {
    WavAudioData wav{};
    if (!data || size < sizeof(RiffHeader) + sizeof(ChunkHeader) + sizeof(FmtChunk)) {
        std::cerr << "[WavLoader] Buffer too small for WAV header\n";
        return wav;
    }

    const auto* riff = reinterpret_cast<const RiffHeader*>(data);
    if (std::memcmp(riff->riff, "RIFF", 4) != 0 || std::memcmp(riff->wave, "WAVE", 4) != 0) {
        std::cerr << "[WavLoader] Not a valid RIFF WAVE buffer\n";
        return wav;
    }

    size_t offset = sizeof(RiffHeader);
    bool found_fmt = false;
    bool found_data = false;

    FmtChunk fmt{};

    while (offset + sizeof(ChunkHeader) <= size) {
        const auto* chunk = reinterpret_cast<const ChunkHeader*>(data + offset);
        offset += sizeof(ChunkHeader);

        if (std::memcmp(chunk->id, "fmt ", 4) == 0) {
            if (chunk->size >= sizeof(FmtChunk) && offset + sizeof(FmtChunk) <= size) {
                std::memcpy(&fmt, data + offset, sizeof(FmtChunk));
                found_fmt = true;
            }
        } else if (std::memcmp(chunk->id, "data", 4) == 0) {
            if (!found_fmt || fmt.bits_per_sample != 16) {
                std::cerr << "[WavLoader] Unsupported or missing fmt chunk before data chunk\n";
                return wav;
            }
            if (offset + chunk->size <= size) {
                size_t num_samples = chunk->size / sizeof(s16);
                wav.pcm_samples.resize(num_samples);
                std::memcpy(wav.pcm_samples.data(), data + offset, num_samples * sizeof(s16));
                found_data = true;
            }
        }

        offset += chunk->size;
        // Chunks are 2-byte aligned
        if (chunk->size % 2 != 0) {
            offset++;
        }
    }

    if (!found_fmt || !found_data || fmt.format_tag != 1) {
        std::cerr << "[WavLoader] Failed to parse PCM data chunk\n";
        return wav;
    }

    wav.sample_rate = fmt.samples_per_sec;
    wav.channels = fmt.channels;
    wav.bits_per_sample = fmt.bits_per_sample;
    wav.is_valid = true;
    return wav;
}

WavAudioData WavLoader::load_from_file(const std::filesystem::path& path) {
    std::ifstream file(path, std::ios::binary | std::ios::ate);
    if (!file.is_open()) {
        std::cerr << "[WavLoader] Failed to open: " << path << "\n";
        return {};
    }

    auto file_size = static_cast<size_t>(file.tellg());
    file.seekg(0, std::ios::beg);

    std::vector<u8> buffer(file_size);
    file.read(reinterpret_cast<char*>(buffer.data()), file_size);

    return load_from_memory(buffer.data(), file_size);
}

} // namespace hotd3::audio
