// hotd3-re — The House of the Dead III: Reverse Engineered
// SPDX-License-Identifier: GPL-3.0-or-later

#include "formats/dds_loader.h"
#include <fstream>
#include <cstring>
#include <iostream>

namespace hotd3::formats {

#pragma pack(push, 1)
struct DdsPixelFormat {
    u32 size;
    u32 flags;
    u32 four_cc;
    u32 rgb_bit_count;
    u32 r_bit_mask;
    u32 g_bit_mask;
    u32 b_bit_mask;
    u32 a_bit_mask;
};

struct DdsHeader {
    u32 magic;              // "DDS " (0x20534444)
    u32 size;               // 124
    u32 flags;
    u32 height;
    u32 width;
    u32 pitch_or_linear_size;
    u32 depth;
    u32 mip_map_count;
    u32 reserved1[11];
    DdsPixelFormat ddspf;
    u32 caps;
    u32 caps2;
    u32 caps3;
    u32 caps4;
    u32 reserved2;
};
#pragma pack(pop)

constexpr u32 FOURCC(char a, char b, char c, char d) {
    return (static_cast<u32>(a)) |
           (static_cast<u32>(b) << 8) |
           (static_cast<u32>(c) << 16) |
           (static_cast<u32>(d) << 24);
}

ParsedTexture DdsLoader::load_from_memory(const u8* data, size_t size) {
    ParsedTexture tex{};
    if (!data || size < sizeof(DdsHeader)) {
        std::cerr << "[DdsLoader] Data buffer too small for DDS header\n";
        return tex;
    }

    const auto* header = reinterpret_cast<const DdsHeader*>(data);
    if (header->magic != FOURCC('D', 'D', 'S', ' ')) {
        std::cerr << "[DdsLoader] Invalid DDS magic\n";
        return tex;
    }

    tex.width = header->width;
    tex.height = header->height;
    tex.mip_levels = (header->mip_map_count > 0) ? header->mip_map_count : 1;

    // Detect format
    if (header->ddspf.flags & 0x00000004) { // DDPF_FOURCC
        switch (header->ddspf.four_cc) {
            case FOURCC('D', 'X', 'T', '1'):
                tex.format = TextureFormat::DXT1;
                break;
            case FOURCC('D', 'X', 'T', '3'):
                tex.format = TextureFormat::DXT3;
                break;
            case FOURCC('D', 'X', 'T', '5'):
                tex.format = TextureFormat::DXT5;
                break;
            default:
                std::cerr << "[DdsLoader] Unsupported FourCC: " << header->ddspf.four_cc << "\n";
                return tex;
        }
    } else if (header->ddspf.flags & 0x00000040) { // DDPF_RGB
        if (header->ddspf.rgb_bit_count == 32) {
            tex.format = (header->ddspf.r_bit_mask == 0x00FF0000) ? TextureFormat::BGRA8 : TextureFormat::RGBA8;
        } else {
            std::cerr << "[DdsLoader] Unsupported RGB bit count: " << header->ddspf.rgb_bit_count << "\n";
            return tex;
        }
    } else {
        std::cerr << "[DdsLoader] Unknown DDS pixel format flags: " << header->ddspf.flags << "\n";
        return tex;
    }

    size_t data_offset = sizeof(DdsHeader);
    size_t payload_size = size - data_offset;
    tex.pixel_data.resize(payload_size);
    std::memcpy(tex.pixel_data.data(), data + data_offset, payload_size);

    tex.is_valid = true;
    return tex;
}

ParsedTexture DdsLoader::load_from_file(const std::string& path) {
    std::ifstream file(path, std::ios::binary | std::ios::ate);
    if (!file.is_open()) {
        std::cerr << "[DdsLoader] Failed to open: " << path << "\n";
        return {};
    }

    auto size = static_cast<size_t>(file.tellg());
    file.seekg(0, std::ios::beg);

    std::vector<u8> buffer(size);
    file.read(reinterpret_cast<char*>(buffer.data()), size);

    return load_from_memory(buffer.data(), size);
}

} // namespace hotd3::formats
