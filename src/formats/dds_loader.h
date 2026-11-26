// hotd3-re — The House of the Dead III: Reverse Engineered
// SPDX-License-Identifier: GPL-3.0-or-later
//
// DirectDraw Surface (.dds) texture parser

#pragma once

#include "core/types.h"
#include "render/renderer.h"
#include <vector>
#include <string>

namespace hotd3::formats {

struct ParsedTexture {
    u32 width = 0;
    u32 height = 0;
    u32 mip_levels = 1;
    TextureFormat format = TextureFormat::Unknown;
    std::vector<u8> pixel_data;
    bool is_valid = false;
};

class DdsLoader {
public:
    static ParsedTexture load_from_memory(const u8* data, size_t size);
    static ParsedTexture load_from_file(const std::string& path);
};

} // namespace hotd3::formats
