// hotd3-re — The House of the Dead III: Reverse Engineered
// SPDX-License-Identifier: GPL-3.0-or-later

#ifdef HOTD3_HAS_DX11

#include "render/dx11/dx11_renderer.h"

#include <SDL.h>
#include <SDL_syswm.h>
#include <cstdio>

namespace hotd3 {

DX11Renderer::DX11Renderer() = default;

DX11Renderer::~DX11Renderer() {
    shutdown();
}

bool DX11Renderer::init(SDL_Window* window, const RenderSettings& settings) {
    settings_ = settings;
    width_    = settings.width;
    height_   = settings.height;
    vsync_    = settings.vsync;

    if (!create_device_and_swap_chain(window)) return false;
    if (!create_render_target()) return false;
    if (!create_depth_buffer()) return false;
    if (!create_pipeline_state()) return false;
    if (!compile_basic_shaders()) return false;

    // Set viewport
    D3D11_VIEWPORT vp{};
    vp.TopLeftX = 0;
    vp.TopLeftY = 0;
    vp.Width    = static_cast<float>(width_);
    vp.Height   = static_cast<float>(height_);
    vp.MinDepth = 0.0f;
    vp.MaxDepth = 1.0f;
    context_->RSSetViewports(1, &vp);

    std::printf("[DX11] Initialized (%ux%u)\n", width_, height_);
    return true;
}

void DX11Renderer::shutdown() {
    if (context_) {
        context_->ClearState();
        context_->Flush();
    }

    // Release all COM objects (ComPtr handles this automatically)
    rtv_.Reset();
    dsv_.Reset();
    depth_buffer_.Reset();
    swap_chain_.Reset();
    context_.Reset();
    device_.Reset();

    std::printf("[DX11] Shut down\n");
}

bool DX11Renderer::create_device_and_swap_chain(SDL_Window* window) {
    // Get HWND from SDL window
    SDL_SysWMinfo wminfo{};
    SDL_VERSION(&wminfo.version);
    if (!SDL_GetWindowWMInfo(window, &wminfo)) {
        std::fprintf(stderr, "[DX11] Failed to get window info: %s\n", SDL_GetError());
        return false;
    }
    HWND hwnd = wminfo.info.win.window;

    // Create DXGI factory
    ComPtr<IDXGIFactory2> factory;
    HRESULT hr = CreateDXGIFactory1(IID_PPV_ARGS(&factory));
    if (FAILED(hr)) {
        std::fprintf(stderr, "[DX11] CreateDXGIFactory1 failed: 0x%08X\n", hr);
        return false;
    }

    // Feature levels to try
    D3D_FEATURE_LEVEL feature_levels[] = {
        D3D_FEATURE_LEVEL_11_1,
        D3D_FEATURE_LEVEL_11_0,
    };

    UINT flags = 0;
#ifdef _DEBUG
    flags |= D3D11_CREATE_DEVICE_DEBUG;
#endif

    D3D_FEATURE_LEVEL achieved_level;
    hr = D3D11CreateDevice(
        nullptr,                    // Default adapter
        D3D_DRIVER_TYPE_HARDWARE,
        nullptr,                    // No software rasterizer
        flags,
        feature_levels,
        _countof(feature_levels),
        D3D11_SDK_VERSION,
        &device_,
        &achieved_level,
        &context_
    );

    if (FAILED(hr)) {
        std::fprintf(stderr, "[DX11] D3D11CreateDevice failed: 0x%08X\n", hr);
        return false;
    }

    std::printf("[DX11] Feature level: %s\n",
                achieved_level == D3D_FEATURE_LEVEL_11_1 ? "11.1" : "11.0");

    // Create swap chain
    DXGI_SWAP_CHAIN_DESC1 scd{};
    scd.Width       = width_;
    scd.Height      = height_;
    scd.Format      = DXGI_FORMAT_R8G8B8A8_UNORM;
    scd.SampleDesc  = {1, 0};
    scd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    scd.BufferCount = 2;
    scd.SwapEffect  = DXGI_SWAP_EFFECT_FLIP_DISCARD;
    scd.Flags       = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;

    hr = factory->CreateSwapChainForHwnd(device_.Get(), hwnd, &scd,
                                         nullptr, nullptr, &swap_chain_);
    if (FAILED(hr)) {
        std::fprintf(stderr, "[DX11] CreateSwapChain failed: 0x%08X\n", hr);
        return false;
    }

    // Disable Alt+Enter fullscreen toggle (we handle it ourselves)
    factory->MakeWindowAssociation(hwnd, DXGI_MWA_NO_ALT_ENTER);

    return true;
}

bool DX11Renderer::create_render_target() {
    ComPtr<ID3D11Texture2D> back_buffer;
    HRESULT hr = swap_chain_->GetBuffer(0, IID_PPV_ARGS(&back_buffer));
    if (FAILED(hr)) return false;

    hr = device_->CreateRenderTargetView(back_buffer.Get(), nullptr, &rtv_);
    return SUCCEEDED(hr);
}

bool DX11Renderer::create_depth_buffer() {
    D3D11_TEXTURE2D_DESC desc{};
    desc.Width      = width_;
    desc.Height     = height_;
    desc.MipLevels  = 1;
    desc.ArraySize  = 1;
    desc.Format     = DXGI_FORMAT_D24_UNORM_S8_UINT;
    desc.SampleDesc = {1, 0};
    desc.Usage      = D3D11_USAGE_DEFAULT;
    desc.BindFlags  = D3D11_BIND_DEPTH_STENCIL;

    HRESULT hr = device_->CreateTexture2D(&desc, nullptr, &depth_buffer_);
    if (FAILED(hr)) return false;

    hr = device_->CreateDepthStencilView(depth_buffer_.Get(), nullptr, &dsv_);
    return SUCCEEDED(hr);
}

bool DX11Renderer::create_pipeline_state() {
    // Rasterizer state
    D3D11_RASTERIZER_DESC rast_desc{};
    rast_desc.FillMode = D3D11_FILL_SOLID;
    rast_desc.CullMode = D3D11_CULL_BACK;
    rast_desc.FrontCounterClockwise = FALSE;
    rast_desc.DepthClipEnable = TRUE;
    device_->CreateRasterizerState(&rast_desc, &rasterizer_state_);

    // Depth stencil state
    D3D11_DEPTH_STENCIL_DESC ds_desc{};
    ds_desc.DepthEnable    = TRUE;
    ds_desc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ALL;
    ds_desc.DepthFunc      = D3D11_COMPARISON_LESS;
    device_->CreateDepthStencilState(&ds_desc, &depth_state_);

    // Blend state (alpha blending for sprites/HUD)
    D3D11_BLEND_DESC blend_desc{};
    blend_desc.RenderTarget[0].BlendEnable    = TRUE;
    blend_desc.RenderTarget[0].SrcBlend       = D3D11_BLEND_SRC_ALPHA;
    blend_desc.RenderTarget[0].DestBlend      = D3D11_BLEND_INV_SRC_ALPHA;
    blend_desc.RenderTarget[0].BlendOp        = D3D11_BLEND_OP_ADD;
    blend_desc.RenderTarget[0].SrcBlendAlpha  = D3D11_BLEND_ONE;
    blend_desc.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_ZERO;
    blend_desc.RenderTarget[0].BlendOpAlpha   = D3D11_BLEND_OP_ADD;
    blend_desc.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
    device_->CreateBlendState(&blend_desc, &blend_state_);

    // Sampler (linear filtering with anisotropic)
    D3D11_SAMPLER_DESC samp_desc{};
    samp_desc.Filter         = D3D11_FILTER_ANISOTROPIC;
    samp_desc.MaxAnisotropy  = 16;
    samp_desc.AddressU       = D3D11_TEXTURE_ADDRESS_WRAP;
    samp_desc.AddressV       = D3D11_TEXTURE_ADDRESS_WRAP;
    samp_desc.AddressW       = D3D11_TEXTURE_ADDRESS_WRAP;
    samp_desc.ComparisonFunc = D3D11_COMPARISON_NEVER;
    samp_desc.MinLOD         = 0;
    samp_desc.MaxLOD         = D3D11_FLOAT32_MAX;
    device_->CreateSamplerState(&samp_desc, &sampler_linear_);

    return true;
}

bool DX11Renderer::compile_basic_shaders() {
    // Basic vertex shader (passthrough with WVP transform)
    const char* vs_source = R"(
        cbuffer cbPerObject : register(b0) {
            float4x4 WorldViewProj;
        };

        struct VS_INPUT {
            float3 position : POSITION;
            float3 normal   : NORMAL;
            float2 texcoord : TEXCOORD0;
            float4 color    : COLOR0;
        };

        struct VS_OUTPUT {
            float4 position : SV_POSITION;
            float3 normal   : NORMAL;
            float2 texcoord : TEXCOORD0;
            float4 color    : COLOR0;
        };

        VS_OUTPUT main(VS_INPUT input) {
            VS_OUTPUT output;
            output.position = mul(float4(input.position, 1.0), WorldViewProj);
            output.normal   = input.normal;
            output.texcoord = input.texcoord;
            output.color    = input.color;
            return output;
        }
    )";

    // Basic pixel shader
    const char* ps_source = R"(
        Texture2D    tex0     : register(t0);
        SamplerState sampler0 : register(s0);

        struct PS_INPUT {
            float4 position : SV_POSITION;
            float3 normal   : NORMAL;
            float2 texcoord : TEXCOORD0;
            float4 color    : COLOR0;
        };

        float4 main(PS_INPUT input) : SV_TARGET {
            float4 tex_color = tex0.Sample(sampler0, input.texcoord);
            return tex_color * input.color;
        }
    )";

    ComPtr<ID3DBlob> vs_blob, ps_blob, errors;

    // Compile vertex shader
    HRESULT hr = D3DCompile(vs_source, strlen(vs_source), "basic_vs",
                            nullptr, nullptr, "main", "vs_5_0", 0, 0,
                            &vs_blob, &errors);
    if (FAILED(hr)) {
        if (errors) {
            std::fprintf(stderr, "[DX11] VS compile error: %s\n",
                        (char*)errors->GetBufferPointer());
        }
        return false;
    }

    hr = device_->CreateVertexShader(vs_blob->GetBufferPointer(),
                                     vs_blob->GetBufferSize(),
                                     nullptr, &basic_vs_);
    if (FAILED(hr)) return false;

    // Compile pixel shader
    hr = D3DCompile(ps_source, strlen(ps_source), "basic_ps",
                    nullptr, nullptr, "main", "ps_5_0", 0, 0,
                    &ps_blob, &errors);
    if (FAILED(hr)) {
        if (errors) {
            std::fprintf(stderr, "[DX11] PS compile error: %s\n",
                        (char*)errors->GetBufferPointer());
        }
        return false;
    }

    hr = device_->CreatePixelShader(ps_blob->GetBufferPointer(),
                                    ps_blob->GetBufferSize(),
                                    nullptr, &basic_ps_);
    if (FAILED(hr)) return false;

    // Input layout matching our Vertex struct
    D3D11_INPUT_ELEMENT_DESC layout[] = {
        {"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT,    0,  0, D3D11_INPUT_PER_VERTEX_DATA, 0},
        {"NORMAL",   0, DXGI_FORMAT_R32G32B32_FLOAT,    0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0},
        {"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT,       0, 24, D3D11_INPUT_PER_VERTEX_DATA, 0},
        {"COLOR",    0, DXGI_FORMAT_R32G32B32A32_FLOAT,  0, 32, D3D11_INPUT_PER_VERTEX_DATA, 0},
    };

    hr = device_->CreateInputLayout(layout, _countof(layout),
                                    vs_blob->GetBufferPointer(),
                                    vs_blob->GetBufferSize(),
                                    &basic_layout_);
    if (FAILED(hr)) return false;

    std::printf("[DX11] Basic shaders compiled successfully\n");
    return true;
}

// ── Frame ────────────────────────────────────────────────────────────────────

void DX11Renderer::begin_frame(const Color& clear_color) {
    float cc[4] = {clear_color.r, clear_color.g, clear_color.b, clear_color.a};
    context_->ClearRenderTargetView(rtv_.Get(), cc);
    context_->ClearDepthStencilView(dsv_.Get(), D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0);

    context_->OMSetRenderTargets(1, rtv_.GetAddressOf(), dsv_.Get());
    context_->RSSetState(rasterizer_state_.Get());
    context_->OMSetDepthStencilState(depth_state_.Get(), 0);

    float blend_factor[4] = {0, 0, 0, 0};
    context_->OMSetBlendState(blend_state_.Get(), blend_factor, 0xFFFFFFFF);

    context_->PSSetSamplers(0, 1, sampler_linear_.GetAddressOf());

    context_->IASetInputLayout(basic_layout_.Get());
    context_->VSSetShader(basic_vs_.Get(), nullptr, 0);
    context_->PSSetShader(basic_ps_.Get(), nullptr, 0);
    context_->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
}

void DX11Renderer::end_frame() {
    swap_chain_->Present(vsync_ ? 1 : 0, 0);
}

// ── Resource management (stubs to be implemented) ────────────────────────────

MeshHandle DX11Renderer::upload_mesh(const MeshData& data) {
    // TODO: Create vertex + index buffers
    MeshHandle h;
    h.id = next_mesh_id_++;
    return h;
}

void DX11Renderer::destroy_mesh(MeshHandle handle) {
    // TODO: Release vertex + index buffers
}

void DX11Renderer::draw_mesh(MeshHandle mesh, const Mat4& transform,
                              TextureHandle texture) {
    // TODO: Set constant buffer, bind mesh, draw
}

void DX11Renderer::draw_sprite(const Sprite& sprite, const Vec2& position,
                                f32 rotation, f32 scale) {
    // TODO: 2D sprite rendering for HUD
}

void DX11Renderer::draw_rect(const RectF& rect, const Color& color, bool filled) {
    // TODO: 2D rectangle rendering
}

TextureHandle DX11Renderer::load_texture(const void* data, u32 width, u32 height,
                                          TextureFormat format) {
    // TODO: Create SRV from raw pixel data
    TextureHandle h;
    h.id = next_texture_id_++;
    return h;
}

TextureHandle DX11Renderer::load_texture_from_file(const std::string& path) {
    // TODO: Load image file (stb_image) and create texture
    return {};
}

void DX11Renderer::destroy_texture(TextureHandle handle) {
    // TODO: Release SRV
}

void DX11Renderer::set_view_matrix(const Mat4& view) {
    // TODO: Update per-frame constant buffer
}

void DX11Renderer::set_projection_matrix(const Mat4& proj) {
    // TODO: Update per-frame constant buffer
}

void DX11Renderer::get_resolution(u32& w, u32& h) const {
    w = width_;
    h = height_;
}

void DX11Renderer::resize(u32 width, u32 height) {
    if (width == 0 || height == 0) return;

    width_ = width;
    height_ = height;

    // Release old targets
    release_render_target();

    // Resize swap chain
    swap_chain_->ResizeBuffers(0, width, height, DXGI_FORMAT_UNKNOWN, 0);

    // Recreate targets
    create_render_target();
    create_depth_buffer();

    // Update viewport
    D3D11_VIEWPORT vp{};
    vp.Width  = static_cast<float>(width);
    vp.Height = static_cast<float>(height);
    vp.MaxDepth = 1.0f;
    context_->RSSetViewports(1, &vp);

    std::printf("[DX11] Resized to %ux%u\n", width, height);
}

void DX11Renderer::set_vsync(bool enabled) {
    vsync_ = enabled;
}

void DX11Renderer::release_render_target() {
    context_->OMSetRenderTargets(0, nullptr, nullptr);
    rtv_.Reset();
    dsv_.Reset();
    depth_buffer_.Reset();
}

} // namespace hotd3

#endif // HOTD3_HAS_DX11
