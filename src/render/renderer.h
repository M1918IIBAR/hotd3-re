// hotd3-re — The House of the Dead III: Reverse Engineered
// SPDX-License-Identifier: GPL-3.0-or-later
//
// Abstract renderer interface. All rendering backends (DX11, Vulkan)
// implement this interface.

#pragma once

#include "core/types.h"
#include <string>
#include <memory>

// Forward declare SDL types
struct SDL_Window;

namespace hotd3 {

// ── Texture format ───────────────────────────────────────────────────────────
enum class TextureFormat : u32 {
    RGBA8,
    BGRA8,
    DXT1,       // BC1
    DXT3,       // BC2
    DXT5,       // BC3
    R8,
    Unknown
};

// ── Vertex layout ────────────────────────────────────────────────────────────
struct Vertex {
    Vec3 position;
    Vec3 normal;
    Vec2 texcoord;
    Color color;
};

// ── Mesh data ────────────────────────────────────────────────────────────────
struct MeshData {
    std::vector<Vertex> vertices;
    std::vector<u32>    indices;
};

// ── Sprite (2D) ──────────────────────────────────────────────────────────────
struct Sprite {
    TextureHandle texture;
    RectF         src_rect;    // UV region in texture
    Vec2          size;        // Display size in pixels
    Vec2          origin;      // Pivot point (0,0 = top-left)
    Color         tint = Color::white();
};

// ── Render settings ──────────────────────────────────────────────────────────
struct RenderSettings {
    u32  width         = 1280;
    u32  height        = 960;
    bool fullscreen    = false;
    bool vsync         = true;
    u32  msaa_samples  = 1;      // 1 = off, 2/4/8
    bool hdr           = false;
    u32  max_fps       = 0;      // 0 = unlimited
};

// ── Renderer backend type ────────────────────────────────────────────────────
enum class RendererBackend : u32 {
    None,
    DirectX11,
    Vulkan
};

// ── Abstract Renderer Interface ──────────────────────────────────────────────
class IRenderer {
public:
    virtual ~IRenderer() = default;

    // Lifecycle
    virtual bool init(SDL_Window* window, const RenderSettings& settings) = 0;
    virtual void shutdown() = 0;

    // Frame
    virtual void begin_frame(const Color& clear_color = Color::black()) = 0;
    virtual void end_frame() = 0;

    // 3D rendering
    virtual MeshHandle   upload_mesh(const MeshData& data) = 0;
    virtual void         destroy_mesh(MeshHandle handle) = 0;
    virtual void         draw_mesh(MeshHandle mesh, const Mat4& transform,
                                   TextureHandle texture = {}) = 0;

    // 2D rendering (HUD, UI)
    virtual void         draw_sprite(const Sprite& sprite, const Vec2& position,
                                     f32 rotation = 0.0f, f32 scale = 1.0f) = 0;
    virtual void         draw_rect(const RectF& rect, const Color& color,
                                   bool filled = true) = 0;

    // Textures
    virtual TextureHandle load_texture(const void* data, u32 width, u32 height,
                                       TextureFormat format) = 0;
    virtual TextureHandle load_texture_from_file(const std::string& path) = 0;
    virtual void          destroy_texture(TextureHandle handle) = 0;

    // Camera / view
    virtual void set_view_matrix(const Mat4& view) = 0;
    virtual void set_projection_matrix(const Mat4& proj) = 0;

    // Info
    virtual RendererBackend backend() const = 0;
    virtual std::string     backend_name() const = 0;
    virtual void            get_resolution(u32& w, u32& h) const = 0;

    // Settings
    virtual void resize(u32 width, u32 height) = 0;
    virtual void set_vsync(bool enabled) = 0;
};

// ── Factory ──────────────────────────────────────────────────────────────────
// Creates the appropriate renderer backend
std::unique_ptr<IRenderer> create_renderer(RendererBackend backend);

} // namespace hotd3
