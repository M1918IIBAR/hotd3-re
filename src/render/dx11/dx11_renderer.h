// hotd3-re — The House of the Dead III: Reverse Engineered
// SPDX-License-Identifier: GPL-3.0-or-later
//
// DirectX 11 rendering backend

#pragma once

#ifdef HOTD3_HAS_DX11

#include "render/renderer.h"

#include <d3d11.h>
#include <dxgi1_2.h>
#include <d3dcompiler.h>
#include <wrl/client.h>  // ComPtr

namespace hotd3 {

using Microsoft::WRL::ComPtr;

class DX11Renderer : public IRenderer {
public:
    DX11Renderer();
    ~DX11Renderer() override;

    // IRenderer interface
    bool init(SDL_Window* window, const RenderSettings& settings) override;
    void shutdown() override;

    void begin_frame(const Color& clear_color) override;
    void end_frame() override;

    MeshHandle   upload_mesh(const MeshData& data) override;
    void         destroy_mesh(MeshHandle handle) override;
    void         draw_mesh(MeshHandle mesh, const Mat4& transform,
                           TextureHandle texture) override;

    void         draw_sprite(const Sprite& sprite, const Vec2& position,
                             f32 rotation, f32 scale) override;
    void         draw_rect(const RectF& rect, const Color& color, bool filled) override;

    TextureHandle load_texture(const void* data, u32 width, u32 height,
                               TextureFormat format) override;
    TextureHandle load_texture_from_file(const std::string& path) override;
    void          destroy_texture(TextureHandle handle) override;

    void set_view_matrix(const Mat4& view) override;
    void set_projection_matrix(const Mat4& proj) override;

    RendererBackend backend() const override { return RendererBackend::DirectX11; }
    std::string     backend_name() const override { return "DirectX 11"; }
    void            get_resolution(u32& w, u32& h) const override;

    void resize(u32 width, u32 height) override;
    void set_vsync(bool enabled) override;

private:
    // D3D11 core objects
    ComPtr<ID3D11Device>           device_;
    ComPtr<ID3D11DeviceContext>    context_;
    ComPtr<IDXGISwapChain1>        swap_chain_;
    ComPtr<ID3D11RenderTargetView> rtv_;
    ComPtr<ID3D11DepthStencilView> dsv_;
    ComPtr<ID3D11Texture2D>        depth_buffer_;

    // Pipeline state
    ComPtr<ID3D11RasterizerState>   rasterizer_state_;
    ComPtr<ID3D11DepthStencilState> depth_state_;
    ComPtr<ID3D11BlendState>        blend_state_;
    ComPtr<ID3D11SamplerState>      sampler_linear_;

    // Basic shaders (to be expanded)
    ComPtr<ID3D11VertexShader>  basic_vs_;
    ComPtr<ID3D11PixelShader>   basic_ps_;
    ComPtr<ID3D11InputLayout>   basic_layout_;

    // Constant buffers
    ComPtr<ID3D11Buffer>        cb_per_frame_;
    ComPtr<ID3D11Buffer>        cb_per_object_;

    // Settings
    RenderSettings settings_;
    bool           vsync_ = true;
    u32            width_ = 0;
    u32            height_ = 0;

    // Resource tracking
    u32 next_texture_id_ = 1;
    u32 next_mesh_id_ = 1;

    // Internal methods
    bool create_device_and_swap_chain(SDL_Window* window);
    bool create_render_target();
    bool create_depth_buffer();
    bool create_pipeline_state();
    bool compile_basic_shaders();

    void release_render_target();
};

} // namespace hotd3

#endif // HOTD3_HAS_DX11
