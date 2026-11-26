// hotd3-re — The House of the Dead III: Reverse Engineered
// SPDX-License-Identifier: GPL-3.0-or-later
//
// Shared shader constant buffer layouts and shader identifiers

#pragma once

#include "core/types.h"

namespace hotd3 {

// Register(b0) - Updated once per frame
struct alignas(16) FrameConstants {
    Mat4 view;
    Mat4 projection;
    Vec4 camera_pos;       // xyz: position, w: unused
    Vec4 light_dir;         // xyz: normalized light direction, w: intensity
    Vec4 light_color;       // rgb: color, a: ambient intensity
    Vec4 screen_params;     // x: width, y: height, z: time, w: delta_time
};

// Register(b1) - Updated per object/draw call
struct alignas(16) ObjectConstants {
    Mat4 world;
    Vec4 diffuse_tint;     // rgba tint multiplier
    Vec4 material_params;  // x: specular power, y: metallic, z: roughness, w: alpha cutoff
};

// Register(b2) - Updated for 2D UI / sprite rendering
struct alignas(16) SpriteConstants {
    Mat4 projection_2d;    // Orthographic projection matrix
    Vec4 tint;             // Sprite color tint
};

// Register(b3) - Skinned mesh bone matrices (up to 128 bones)
struct alignas(16) SkinnedConstants {
    Mat4 bone_matrices[128];
};

} // namespace hotd3
