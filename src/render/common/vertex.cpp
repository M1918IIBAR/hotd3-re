// hotd3-re — The House of the Dead III: Reverse Engineered
// SPDX-License-Identifier: GPL-3.0-or-later

#include "render/common/vertex.h"

namespace hotd3 {

namespace MeshBuilder {

MeshData create_cube(f32 size) {
    f32 h = size * 0.5f;
    MeshData mesh;

    mesh.vertices = {
        // Front face (+Z)
        { { -h, -h,  h }, { 0, 0, 1 }, { 0, 1 }, Color::white() },
        { {  h, -h,  h }, { 0, 0, 1 }, { 1, 1 }, Color::white() },
        { {  h,  h,  h }, { 0, 0, 1 }, { 1, 0 }, Color::white() },
        { { -h,  h,  h }, { 0, 0, 1 }, { 0, 0 }, Color::white() },
        // Back face (-Z)
        { {  h, -h, -h }, { 0, 0, -1 }, { 0, 1 }, Color::white() },
        { { -h, -h, -h }, { 0, 0, -1 }, { 1, 1 }, Color::white() },
        { { -h,  h, -h }, { 0, 0, -1 }, { 1, 0 }, Color::white() },
        { {  h,  h, -h }, { 0, 0, -1 }, { 0, 0 }, Color::white() },
        // Top face (+Y)
        { { -h,  h,  h }, { 0, 1, 0 }, { 0, 1 }, Color::white() },
        { {  h,  h,  h }, { 0, 1, 0 }, { 1, 1 }, Color::white() },
        { {  h,  h, -h }, { 0, 1, 0 }, { 1, 0 }, Color::white() },
        { { -h,  h, -h }, { 0, 1, 0 }, { 0, 0 }, Color::white() },
        // Bottom face (-Y)
        { { -h, -h, -h }, { 0, -1, 0 }, { 0, 1 }, Color::white() },
        { {  h, -h, -h }, { 0, -1, 0 }, { 1, 1 }, Color::white() },
        { {  h, -h,  h }, { 0, -1, 0 }, { 1, 0 }, Color::white() },
        { { -h, -h,  h }, { 0, -1, 0 }, { 0, 0 }, Color::white() },
        // Right face (+X)
        { {  h, -h,  h }, { 1, 0, 0 }, { 0, 1 }, Color::white() },
        { {  h, -h, -h }, { 1, 0, 0 }, { 1, 1 }, Color::white() },
        { {  h,  h, -h }, { 1, 0, 0 }, { 1, 0 }, Color::white() },
        { {  h,  h,  h }, { 1, 0, 0 }, { 0, 0 }, Color::white() },
        // Left face (-X)
        { { -h, -h, -h }, { -1, 0, 0 }, { 0, 1 }, Color::white() },
        { { -h, -h,  h }, { -1, 0, 0 }, { 1, 1 }, Color::white() },
        { { -h,  h,  h }, { -1, 0, 0 }, { 1, 0 }, Color::white() },
        { { -h,  h, -h }, { -1, 0, 0 }, { 0, 0 }, Color::white() }
    };

    mesh.indices = {
        0, 1, 2,  0, 2, 3,       // Front
        4, 5, 6,  4, 6, 7,       // Back
        8, 9, 10, 8, 10, 11,     // Top
        12, 13, 14, 12, 14, 15,  // Bottom
        16, 17, 18, 16, 18, 19,  // Right
        20, 21, 22, 20, 22, 23   // Left
    };

    return mesh;
}

MeshData create_quad(f32 width, f32 height) {
    f32 hw = width * 0.5f;
    f32 hh = height * 0.5f;
    MeshData mesh;

    mesh.vertices = {
        { { -hw, -hh, 0.0f }, { 0, 0, 1 }, { 0.0f, 1.0f }, Color::white() },
        { {  hw, -hh, 0.0f }, { 0, 0, 1 }, { 1.0f, 1.0f }, Color::white() },
        { {  hw,  hh, 0.0f }, { 0, 0, 1 }, { 1.0f, 0.0f }, Color::white() },
        { { -hw,  hh, 0.0f }, { 0, 0, 1 }, { 0.0f, 0.0f }, Color::white() }
    };

    mesh.indices = { 0, 1, 2, 0, 2, 3 };
    return mesh;
}

MeshData create_screen_quad() {
    MeshData mesh;
    // Normalized Device Coordinates (NDC) fullscreen quad [-1, 1]
    mesh.vertices = {
        { { -1.0f, -1.0f, 0.0f }, { 0, 0, 1 }, { 0.0f, 1.0f }, Color::white() },
        { {  1.0f, -1.0f, 0.0f }, { 0, 0, 1 }, { 1.0f, 1.0f }, Color::white() },
        { {  1.0f,  1.0f, 0.0f }, { 0, 0, 1 }, { 1.0f, 0.0f }, Color::white() },
        { { -1.0f,  1.0f, 0.0f }, { 0, 0, 1 }, { 0.0f, 0.0f }, Color::white() }
    };

    mesh.indices = { 0, 1, 2, 0, 2, 3 };
    return mesh;
}

} // namespace MeshBuilder

} // namespace hotd3
