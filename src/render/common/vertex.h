// hotd3-re — The House of the Dead III: Reverse Engineered
// SPDX-License-Identifier: GPL-3.0-or-later
//
// Vertex definitions and layouts for rendering

#pragma once

#include "core/types.h"
#include "render/renderer.h"
#include <vector>

namespace hotd3 {

// 2D Vertex used for HUD, sprites, UI rectangles
struct Vertex2D {
    Vec2 position;
    Vec2 texcoord;
    Color color = Color::white();

    static constexpr u32 stride() { return sizeof(Vertex2D); }
};

// 3D Skinned Vertex for skeletal character meshes
struct SkinnedVertex {
    Vec3 position;
    Vec3 normal;
    Vec2 texcoord;
    Color color = Color::white();
    f32 bone_weights[4] = { 1.0f, 0.0f, 0.0f, 0.0f };
    u32 bone_indices[4] = { 0, 0, 0, 0 };

    static constexpr u32 stride() { return sizeof(SkinnedVertex); }
};

// Mesh creation helper functions
namespace MeshBuilder {
    MeshData create_cube(f32 size = 1.0f);
    MeshData create_quad(f32 width = 1.0f, f32 height = 1.0f);
    MeshData create_screen_quad();
}

} // namespace hotd3
