// hotd3-re — The House of the Dead III: Reverse Engineered
// SPDX-License-Identifier: GPL-3.0-or-later
//
// Vulkan rendering backend

#pragma once

#ifdef HOTD3_HAS_VULKAN

#include "render/renderer.h"

#include <vulkan/vulkan.h>
#include <unordered_map>
#include <vector>

namespace hotd3 {

class VulkanRenderer : public IRenderer {
public:
    VulkanRenderer();
    ~VulkanRenderer() override;

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

    RendererBackend backend() const override { return RendererBackend::Vulkan; }
    std::string     backend_name() const override { return "Vulkan"; }
    void            get_resolution(u32& w, u32& h) const override;

    void resize(u32 width, u32 height) override;
    void set_vsync(bool enabled) override;

private:
    static constexpr u32 MAX_FRAMES_IN_FLIGHT = 2;

    struct PushConstants {
        Mat4 mvp;
        Color tint;
    };

    struct VkMesh {
        VkBuffer       vb = VK_NULL_HANDLE;
        VkDeviceMemory vb_mem = VK_NULL_HANDLE;
        VkBuffer       ib = VK_NULL_HANDLE;
        VkDeviceMemory ib_mem = VK_NULL_HANDLE;
        u32            index_count = 0;
    };

    struct VkTexture {
        VkImage         image = VK_NULL_HANDLE;
        VkDeviceMemory  memory = VK_NULL_HANDLE;
        VkImageView     view = VK_NULL_HANDLE;
        VkDescriptorSet descriptor_set = VK_NULL_HANDLE;
        u32             width = 0;
        u32             height = 0;
    };

    SDL_Window* window_ = nullptr;
    RenderSettings settings_{};
    u32 width_ = 0;
    u32 height_ = 0;
    bool vsync_ = true;

    // Core Vulkan objects
    VkInstance       instance_ = VK_NULL_HANDLE;
    VkSurfaceKHR     surface_ = VK_NULL_HANDLE;
    VkPhysicalDevice physical_device_ = VK_NULL_HANDLE;
    VkDevice         device_ = VK_NULL_HANDLE;
    VkQueue          graphics_queue_ = VK_NULL_HANDLE;
    VkQueue          present_queue_ = VK_NULL_HANDLE;
    u32              graphics_family_ = 0;
    u32              present_family_ = 0;

    // Swapchain & attachments
    VkSwapchainKHR             swapchain_ = VK_NULL_HANDLE;
    VkFormat                   swapchain_format_ = VK_FORMAT_B8G8R8A8_UNORM;
    VkExtent2D                 swapchain_extent_{};
    std::vector<VkImage>       swapchain_images_;
    std::vector<VkImageView>   swapchain_views_;
    std::vector<VkFramebuffer> framebuffers_;

    VkFormat       depth_format_ = VK_FORMAT_D32_SFLOAT;
    VkImage        depth_image_ = VK_NULL_HANDLE;
    VkDeviceMemory depth_memory_ = VK_NULL_HANDLE;
    VkImageView    depth_view_ = VK_NULL_HANDLE;

    VkRenderPass          render_pass_ = VK_NULL_HANDLE;
    VkDescriptorSetLayout descriptor_layout_ = VK_NULL_HANDLE;
    VkDescriptorPool      descriptor_pool_ = VK_NULL_HANDLE;
    VkPipelineLayout      pipeline_layout_ = VK_NULL_HANDLE;
    VkPipeline            graphics_pipeline_ = VK_NULL_HANDLE;
    VkSampler             sampler_linear_ = VK_NULL_HANDLE;

    // Command buffers & synchronization
    VkCommandPool                command_pool_ = VK_NULL_HANDLE;
    std::vector<VkCommandBuffer> command_buffers_;
    std::vector<VkSemaphore>     image_available_semaphores_;
    std::vector<VkSemaphore>     render_finished_semaphores_;
    std::vector<VkFence>         in_flight_fences_;
    u32                          current_frame_ = 0;
    u32                          current_image_index_ = 0;
    bool                         frame_active_ = false;
    bool                         framebuffer_resized_ = false;

    // Camera matrices
    Mat4 view_matrix_ = Mat4::identity();
    Mat4 proj_matrix_ = Mat4::identity();

    // Resources
    u32 next_mesh_id_ = 1;
    u32 next_texture_id_ = 1;
    std::unordered_map<u32, VkMesh>    meshes_;
    std::unordered_map<u32, VkTexture> textures_;
    TextureHandle default_white_tex_{};
    MeshHandle    unit_quad_mesh_{};

    // Initialization helpers
    bool create_instance(SDL_Window* window);
    bool pick_physical_device();
    bool create_logical_device();
    bool create_swapchain();
    bool create_render_pass();
    bool create_depth_resources();
    bool create_framebuffers();
    bool create_command_pool_and_buffers();
    bool create_sync_objects();
    bool create_descriptor_resources();
    bool create_graphics_pipeline();
    bool create_default_resources();

    void cleanup_swapchain();
    bool recreate_swapchain();

    // Low-level Vulkan helpers
    u32 find_memory_type(u32 type_filter, VkMemoryPropertyFlags properties) const;
    VkFormat find_depth_format() const;
    bool create_buffer(VkDeviceSize size, VkBufferUsageFlags usage,
                       VkMemoryPropertyFlags properties,
                       VkBuffer& buffer, VkDeviceMemory& memory);
    void copy_buffer(VkBuffer src, VkBuffer dst, VkDeviceSize size);
    VkCommandBuffer begin_single_time_commands();
    void end_single_time_commands(VkCommandBuffer cmd);
    void transition_image_layout(VkImage image, VkImageLayout old_layout,
                                 VkImageLayout new_layout);
    void copy_buffer_to_image(VkBuffer buffer, VkImage image, u32 width, u32 height);
    VkShaderModule create_shader_module(const std::vector<u8>& code);
    void draw_quad_2d(const RectF& rect, const Color& color, TextureHandle texture);
};

} // namespace hotd3

#endif // HOTD3_HAS_VULKAN
