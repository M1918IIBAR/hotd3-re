// hotd3-re — The House of the Dead III: Reverse Engineered
// SPDX-License-Identifier: GPL-3.0-or-later
//
// Renderer factory implementation

#include "render/renderer.h"

#ifdef HOTD3_HAS_DX11
#include "render/dx11/dx11_renderer.h"
#endif

#ifdef HOTD3_HAS_VULKAN
#include "render/vulkan/vk_renderer.h"
#endif

#include <cstdio>

namespace hotd3 {

std::unique_ptr<IRenderer> create_renderer(RendererBackend backend) {
    switch (backend) {
#ifdef HOTD3_HAS_DX11
        case RendererBackend::DirectX11:
            return std::make_unique<DX11Renderer>();
#endif

#ifdef HOTD3_HAS_VULKAN
        case RendererBackend::Vulkan:
            // return std::make_unique<VulkanRenderer>();
            std::fprintf(stderr, "[Renderer] Vulkan backend not yet implemented\n");
            return nullptr;
#endif

        default:
            std::fprintf(stderr, "[Renderer] Unsupported backend requested\n");
            return nullptr;
    }
}

} // namespace hotd3
