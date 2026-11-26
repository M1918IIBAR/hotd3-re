// hotd3-re — The House of the Dead III: Reverse Engineered
// SPDX-License-Identifier: GPL-3.0-or-later

#include "formats/xmx_loader.h"
#include <fstream>
#include <cstring>
#include <iostream>
#include <algorithm>

namespace hotd3::formats {

namespace {

#pragma pack(push, 1)
struct XmxHeader {
    char magic[8];     // "LDMXXOBX" (XMDL XBOX)
    u32 version;
    u32 total_size;
    char submagic[4];  // "LEDM" (MDEL)
};
#pragma pack(pop)

// Direct3D 8 FVF flags used in Chihiro/Xbox XMX vertex streams
constexpr u32 kFvfXyz     = 0x002;
constexpr u32 kFvfNormal  = 0x010;
constexpr u32 kFvfDiffuse = 0x040;
constexpr u32 kFvfTexMask = 0xF00;

inline u32 read_u32(const u8* p) {
    u32 v = 0;
    std::memcpy(&v, p, sizeof(u32));
    return v;
}

inline u16 read_u16(const u8* p) {
    u16 v = 0;
    std::memcpy(&v, p, sizeof(u16));
    return v;
}

inline f32 read_f32(const u8* p) {
    f32 v = 0.0f;
    std::memcpy(&v, p, sizeof(f32));
    return v;
}

// Convert a D3D8 triangle strip index buffer into a triangle list,
// preserving winding order and discarding degenerate triangles.
void append_strip_as_triangles(const u16* strip, u32 index_count, u32 vertex_base,
                               std::vector<u32>& out_indices) {
    if (index_count < 3) return;

    for (u32 i = 0; i + 2 < index_count; ++i) {
        u16 i0 = strip[i];
        u16 i1 = strip[i + 1];
        u16 i2 = strip[i + 2];

        // Skip degenerate restart triangles
        if (i0 == i1 || i1 == i2 || i0 == i2) {
            continue;
        }

        // Alternate winding on odd triangles
        if ((i & 1) == 0) {
            out_indices.push_back(vertex_base + i0);
            out_indices.push_back(vertex_base + i1);
            out_indices.push_back(vertex_base + i2);
        } else {
            out_indices.push_back(vertex_base + i0);
            out_indices.push_back(vertex_base + i2);
            out_indices.push_back(vertex_base + i1);
        }
    }
}

} // namespace

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

    // Scan for VPRG ("GRPV") vertex/index group chunks.
    // Offsets inside VPRG are relative to the LEDM header at byte 0x10.
    constexpr size_t kLedmBase = 0x10;
    constexpr size_t kVprgHeaderMinSize = 0x50;

    for (size_t pos = kLedmBase; pos + kVprgHeaderMinSize <= size; pos += 4) {
        if (std::memcmp(data + pos, "VPRG", 4) != 0) {
            continue;
        }

        const u8* vp = data + pos;
        u32 vertex_count  = read_u32(vp + 0x0C);
        u32 xtrv_rel      = read_u32(vp + 0x14);
        u32 fvf           = read_u32(vp + 0x30);
        u32 vertex_stride = read_u32(vp + 0x34);
        u32 xdni_rel      = read_u32(vp + 0x44);
        u32 index_count   = read_u32(vp + 0x48);

        if (vertex_count == 0 || vertex_stride < 12) {
            continue;
        }

        size_t vtx_offset = kLedmBase + xtrv_rel;
        size_t vtx_bytes  = static_cast<size_t>(vertex_count) * vertex_stride;
        if (vtx_offset + vtx_bytes > size) {
            std::cerr << "[XmxLoader] Vertex buffer extends past EOF\n";
            continue;
        }

        u32 vertex_base = static_cast<u32>(model.mesh.vertices.size());
        u32 index_base  = static_cast<u32>(model.mesh.indices.size());

        // Decode vertices according to D3D8 FVF and stride
        model.mesh.vertices.reserve(model.mesh.vertices.size() + vertex_count);
        for (u32 v = 0; v < vertex_count; ++v) {
            const u8* src = data + vtx_offset + static_cast<size_t>(v) * vertex_stride;
            Vertex vert{};
            vert.color = Color::white();
            size_t off = 0;

            if ((fvf == 0 || (fvf & kFvfXyz)) && off + 12 <= vertex_stride) {
                vert.position.x = read_f32(src + off);
                vert.position.y = read_f32(src + off + 4);
                vert.position.z = read_f32(src + off + 8);
                off += 12;
            }
            if ((fvf == 0 || (fvf & kFvfNormal)) && off + 12 <= vertex_stride) {
                vert.normal.x = read_f32(src + off);
                vert.normal.y = read_f32(src + off + 4);
                vert.normal.z = read_f32(src + off + 8);
                off += 12;
            } else {
                vert.normal = { 0.0f, 1.0f, 0.0f };
            }
            if ((fvf & kFvfDiffuse) && off + 4 <= vertex_stride) {
                // D3DCOLOR is BGRA in little-endian: B, G, R, A bytes
                vert.color.b = src[off + 0] / 255.0f;
                vert.color.g = src[off + 1] / 255.0f;
                vert.color.r = src[off + 2] / 255.0f;
                vert.color.a = src[off + 3] / 255.0f;
                off += 4;
            }
            if ((fvf == 0 || (fvf & kFvfTexMask) >= 0x100) && off + 8 <= vertex_stride) {
                vert.texcoord.x = read_f32(src + off);
                vert.texcoord.y = read_f32(src + off + 4);
                off += 8;
            }

            model.mesh.vertices.push_back(vert);
        }

        // Decode XDNI index buffer (16-bit triangle strip)
        size_t idx_offset = kLedmBase + xdni_rel;
        size_t idx_bytes  = static_cast<size_t>(index_count) * sizeof(u16);
        if (index_count >= 3 && idx_offset + idx_bytes <= size) {
            std::vector<u16> strip(index_count);
            for (u32 i = 0; i < index_count; ++i) {
                strip[i] = read_u16(data + idx_offset + static_cast<size_t>(i) * 2);
            }
            append_strip_as_triangles(strip.data(), index_count, vertex_base, model.mesh.indices);
        }

        XmxSubmesh sub{};
        sub.name          = "submesh_" + std::to_string(model.submeshes.size());
        sub.vertex_offset = vertex_base;
        sub.vertex_count  = vertex_count;
        sub.index_offset  = index_base;
        sub.index_count   = static_cast<u32>(model.mesh.indices.size()) - index_base;
        model.submeshes.push_back(std::move(sub));
    }

    model.is_valid = !model.mesh.vertices.empty();
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
