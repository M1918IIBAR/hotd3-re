// hotd3-re — The House of the Dead III: Reverse Engineered
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <cstdint>
#include <cstddef>
#include <string>
#include <memory>
#include <vector>
#include <array>
#include <unordered_map>
#include <functional>
#include <optional>
#include <filesystem>

namespace hotd3 {

// ── Primitive type aliases ───────────────────────────────────────────────────
using u8  = uint8_t;
using u16 = uint16_t;
using u32 = uint32_t;
using u64 = uint64_t;
using s8  = int8_t;
using s16 = int16_t;
using s32 = int32_t;
using s64 = int64_t;
using f32 = float;
using f64 = double;

// ── Math types (simple, to be replaced with GLM later) ───────────────────────
struct Vec2 {
    f32 x = 0.0f, y = 0.0f;

    Vec2() = default;
    Vec2(f32 x, f32 y) : x(x), y(y) {}

    Vec2 operator+(const Vec2& o) const { return {x + o.x, y + o.y}; }
    Vec2 operator-(const Vec2& o) const { return {x - o.x, y - o.y}; }
    Vec2 operator*(f32 s) const { return {x * s, y * s}; }
};

struct Vec3 {
    f32 x = 0.0f, y = 0.0f, z = 0.0f;

    Vec3() = default;
    Vec3(f32 x, f32 y, f32 z) : x(x), y(y), z(z) {}

    Vec3 operator+(const Vec3& o) const { return {x + o.x, y + o.y, z + o.z}; }
    Vec3 operator-(const Vec3& o) const { return {x - o.x, y - o.y, z - o.z}; }
    Vec3 operator*(f32 s) const { return {x * s, y * s, z * s}; }

    f32 dot(const Vec3& o) const { return x*o.x + y*o.y + z*o.z; }
    Vec3 cross(const Vec3& o) const {
        return {y*o.z - z*o.y, z*o.x - x*o.z, x*o.y - y*o.x};
    }
};

struct Vec4 {
    f32 x = 0.0f, y = 0.0f, z = 0.0f, w = 1.0f;
};

// Row-major 4x4 matrix
struct Mat4 {
    f32 m[4][4] = {};

    static Mat4 identity() {
        Mat4 result{};
        result.m[0][0] = result.m[1][1] = result.m[2][2] = result.m[3][3] = 1.0f;
        return result;
    }
};

// ── Color types ──────────────────────────────────────────────────────────────
struct Color {
    f32 r = 1.0f, g = 1.0f, b = 1.0f, a = 1.0f;

    Color() = default;
    Color(f32 r, f32 g, f32 b, f32 a = 1.0f) : r(r), g(g), b(b), a(a) {}

    static Color white()  { return {1, 1, 1, 1}; }
    static Color black()  { return {0, 0, 0, 1}; }
    static Color red()    { return {1, 0, 0, 1}; }
    static Color green()  { return {0, 1, 0, 1}; }
    static Color blue()   { return {0, 0, 1, 1}; }
    static Color yellow() { return {1, 1, 0, 1}; }
};

// ── Handle types ─────────────────────────────────────────────────────────────
// Opaque handles for renderer resources
struct TextureHandle { u32 id = 0; bool valid() const { return id != 0; } };
struct MeshHandle    { u32 id = 0; bool valid() const { return id != 0; } };
struct ShaderHandle  { u32 id = 0; bool valid() const { return id != 0; } };

// ── Rect ─────────────────────────────────────────────────────────────────────
struct Rect {
    s32 x = 0, y = 0, w = 0, h = 0;
};

struct RectF {
    f32 x = 0, y = 0, w = 0, h = 0;
};

} // namespace hotd3
