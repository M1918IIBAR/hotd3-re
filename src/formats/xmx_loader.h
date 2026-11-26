// hotd3-re — The House of the Dead III: Reverse Engineered
// SPDX-License-Identifier: GPL-3.0-or-later
//
// Sega Chihiro / Xbox Model (.xmx) geometry parser

#pragma once

#include "core/types.h"
#include "render/renderer.h"
#include <vector>
#include <string>

namespace hotd3::formats {

struct XmxSubmesh {
    std::string name;
    u32 vertex_offset = 0;
    u32 vertex_count = 0;
    u32 index_offset = 0;
    u32 index_count = 0;
    u32 material_id = 0;
};

struct ParsedModel {
    MeshData mesh;
    std::vector<XmxSubmesh> submeshes;
    bool is_valid = false;
};

class XmxLoader {
public:
    static ParsedModel load_from_memory(const u8* data, size_t size);
    static ParsedModel load_from_file(const std::string& path);
};

} // namespace hotd3::formats
