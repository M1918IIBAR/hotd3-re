// hotd3-re — The House of the Dead III: Reverse Engineered
// SPDX-License-Identifier: GPL-3.0-or-later

#include "formats/xmx_loader.h"
#include <fstream>
#include <cstring>
#include <iostream>

namespace hotd3::formats {

#pragma pack(push, 1)
struct XmxHeader {
    char magic[8];     // "LDMXXOBX" (XMDL XBOX)
    u32 version;
    u32 total_size;
    char submagic[4];  // "LEDM" (MDEL)
};
#pragma pack(pop)

ParsedModel XmxLoader::load_from_memory(const u8* data, size_t size) {
    ParsedModel model{};
    if (!data || size < sizeof(XmxHeader)) {
        std::cerr << "[XmxLoader] File too small for XMX header\n";
        return model;
    }

    const auto* header = reinterpret_cast<const XmxHeader*>(data);
    if (std::memcmp(header->magic, "LDMXXOBX", 8) != 0) {
        std::cerr << "[XmxLoader] Invalid XMX magic\n";
        return model;
    }

    // Basic structure verified
    model.is_valid = true;
    return model;
}

ParsedModel XmxLoader::load_from_file(const std::string& path) {
    std::ifstream file(path, std::ios::binary | std::ios::ate);
    if (!file.is_open()) {
        std::cerr << "[XmxLoader] Failed to open: " << path << "\n";
        return {};
    }

    auto size = static_cast<size_t>(file.tellg());
    file.seekg(0, std::ios::beg);

    std::vector<u8> buffer(size);
    file.read(reinterpret_cast<char*>(buffer.data()), size);

    return load_from_memory(buffer.data(), size);
}

} // namespace hotd3::formats
