// hotd3-re — The House of the Dead III: Reverse Engineered
// SPDX-License-Identifier: GPL-3.0-or-later

#include "audio/aiff_loader.h"

#include <cstring>
#include <cmath>
#include <fstream>
#include <iostream>

namespace hotd3::audio {

// ── Byte-order helpers ────────────────────────────────────────────────────────

static inline u16 read_be16(const u8* p) {
    return static_cast<u16>((p[0] << 8) | p[1]);
}

static inline u32 read_be32(const u8* p) {
    return (static_cast<u32>(p[0]) << 24) |
           (static_cast<u32>(p[1]) << 16) |
           (static_cast<u32>(p[2]) <<  8) |
            static_cast<u32>(p[3]);
}

// ── 80-bit IEEE 754 extended → double ────────────────────────────────────────
// Used in AIFF COMM chunk for sample rate.

double AiffLoader::extended_to_double(const u8 bytes[10]) {
    // Sign (1 bit) | Exponent (15 bits) | Integer+fraction (64 bits)
    bool     sign     = (bytes[0] & 0x80) != 0;
    int      exponent = ((bytes[0] & 0x7F) << 8) | bytes[1];
    u64      mantissa = 0;
    for (int i = 0; i < 8; ++i) {
        mantissa = (mantissa << 8) | bytes[2 + i];
    }

    if (exponent == 0 && mantissa == 0) return 0.0;

    // Unbias: bias for 80-bit extended is 16383
    exponent -= 16383;

    // The integer bit is explicit in 80-bit extended
    double value = static_cast<double>(mantissa) * std::ldexp(1.0, exponent - 63);
    return sign ? -value : value;
}

// ── Main parse ────────────────────────────────────────────────────────────────

AiffAudioData AiffLoader::load_from_memory(const u8* data, size_t size) {
    AiffAudioData aiff{};

    if (!data || size < 12) {
        std::cerr << "[AiffLoader] Buffer too small\n";
        return aiff;
    }

    // Expect "FORM" <size> "AIFF"
    if (std::memcmp(data, "FORM", 4) != 0) {
        std::cerr << "[AiffLoader] Not an AIFF file (missing FORM)\n";
        return aiff;
    }
    if (std::memcmp(data + 8, "AIFF", 4) != 0) {
        std::cerr << "[AiffLoader] FORM type is not AIFF (AIFF-C not supported yet)\n";
        return aiff;
    }

    bool found_comm = false;
    bool found_ssnd = false;

    size_t pos = 12; // after "FORM" <size> "AIFF"
    while (pos + 8 <= size) {
        char   chunk_id[5]  = {};
        std::memcpy(chunk_id, data + pos, 4);
        u32 chunk_size = read_be32(data + pos + 4);
        pos += 8;

        if (pos + chunk_size > size) {
            std::cerr << "[AiffLoader] Chunk '" << chunk_id
                      << "' extends beyond file boundary\n";
            break;
        }

        const u8* chunk = data + pos;

        if (std::memcmp(chunk_id, "COMM", 4) == 0 && chunk_size >= 18) {
            // COMM: numChannels(2) numSampleFrames(4) sampleSize(2) sampleRate(10)
            aiff.channels         = read_be16(chunk);
            aiff.frame_count      = read_be32(chunk + 2);
            aiff.bits_per_sample  = read_be16(chunk + 6);
            aiff.sample_rate      = static_cast<u32>(extended_to_double(chunk + 8));
            found_comm = true;
        }
        else if (std::memcmp(chunk_id, "SSND", 4) == 0 && chunk_size >= 8) {
            // SSND: offset(4) blockSize(4) sampleData
            u32 data_offset = read_be32(chunk);      // almost always 0
            // u32 block_size = read_be32(chunk + 4); // almost always 0
            const u8* pcm_start = chunk + 8 + data_offset;
            size_t    pcm_bytes = chunk_size - 8 - data_offset;

            aiff.pcm_data.resize(pcm_bytes);
            std::memcpy(aiff.pcm_data.data(), pcm_start, pcm_bytes);

            // Convert big-endian 16-bit samples to host (little-endian) order
            if (aiff.bits_per_sample == 16) {
                for (size_t i = 0; i + 1 < pcm_bytes; i += 2) {
                    u8 tmp = aiff.pcm_data[i];
                    aiff.pcm_data[i]     = aiff.pcm_data[i + 1];
                    aiff.pcm_data[i + 1] = tmp;
                }
            }

            found_ssnd = true;
        }

        // AIFF chunks are padded to even byte boundary
        pos += chunk_size + (chunk_size & 1);
    }

    if (!found_comm) {
        std::cerr << "[AiffLoader] Missing COMM chunk\n";
        return aiff;
    }
    if (!found_ssnd) {
        std::cerr << "[AiffLoader] Missing SSND chunk\n";
        return aiff;
    }

    aiff.is_valid = true;
    return aiff;
}

AiffAudioData AiffLoader::load_from_file(const std::filesystem::path& path) {
    std::ifstream file(path, std::ios::binary | std::ios::ate);
    if (!file.is_open()) {
        std::cerr << "[AiffLoader] Failed to open: " << path << "\n";
        return {};
    }

    auto file_size = static_cast<size_t>(file.tellg());
    file.seekg(0, std::ios::beg);

    std::vector<u8> buffer(file_size);
    file.read(reinterpret_cast<char*>(buffer.data()), file_size);

    return load_from_memory(buffer.data(), file_size);
}

} // namespace hotd3::audio
