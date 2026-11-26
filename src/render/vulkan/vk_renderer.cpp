// hotd3-re — The House of the Dead III: Reverse Engineered
// SPDX-License-Identifier: GPL-3.0-or-later
//
// Vulkan rendering backend implementation

#ifdef HOTD3_HAS_VULKAN

#include "render/vulkan/vk_renderer.h"
#include "formats/dds_loader.h"
#include "render/common/vertex.h"

#include <SDL.h>
#include <SDL_vulkan.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <set>

namespace hotd3 {

namespace {

Mat4 mul_mat4(const Mat4& a, const Mat4& b) {
    Mat4 r{};
    for (int i = 0; i < 4; ++i) {
        for (int j = 0; j < 4; ++j) {
            r.m[i][j] = a.m[i][0] * b.m[0][j] +
                        a.m[i][1] * b.m[1][j] +
                        a.m[i][2] * b.m[2][j] +
                        a.m[i][3] * b.m[3][j];
        }
    }
    return r;
}

Mat4 ortho_2d(f32 width, f32 height) {
    Mat4 m = Mat4::identity();
    m.m[0][0] = 2.0f / width;
    m.m[1][1] = -2.0f / height;
    m.m[3][0] = -1.0f;
    m.m[3][1] = 1.0f;
    return m;
}

VkFormat get_vk_format(TextureFormat format) {
    switch (format) {
        case TextureFormat::RGBA8: return VK_FORMAT_R8G8B8A8_UNORM;
        case TextureFormat::BGRA8: return VK_FORMAT_B8G8R8A8_UNORM;
        case TextureFormat::DXT1:  return VK_FORMAT_BC1_RGBA_UNORM_BLOCK;
        case TextureFormat::DXT3:  return VK_FORMAT_BC2_UNORM_BLOCK;
        case TextureFormat::DXT5:  return VK_FORMAT_BC3_UNORM_BLOCK;
        case TextureFormat::R8:    return VK_FORMAT_R8_UNORM;
        default:                   return VK_FORMAT_UNDEFINED;
    }
}

VkDeviceSize calculate_image_size(TextureFormat format, u32 width, u32 height) {
    switch (format) {
        case TextureFormat::DXT1: {
            u32 bw = std::max(1u, (width + 3) / 4);
            u32 bh = std::max(1u, (height + 3) / 4);
            return static_cast<VkDeviceSize>(bw) * bh * 8;
        }
        case TextureFormat::DXT3:
        case TextureFormat::DXT5: {
            u32 bw = std::max(1u, (width + 3) / 4);
            u32 bh = std::max(1u, (height + 3) / 4);
            return static_cast<VkDeviceSize>(bw) * bh * 16;
        }
        case TextureFormat::RGBA8:
        case TextureFormat::BGRA8:
            return static_cast<VkDeviceSize>(width) * height * 4;
        case TextureFormat::R8:
            return static_cast<VkDeviceSize>(width) * height;
        default:
            return static_cast<VkDeviceSize>(width) * height * 4;
    }
}

std::vector<u8> read_binary_file(const std::string& path) {
    std::ifstream file(path, std::ios::ate | std::ios::binary);
    if (!file.is_open()) return {};
    const auto size = static_cast<size_t>(file.tellg());
    std::vector<u8> buffer(size);
    file.seekg(0);
    file.read(reinterpret_cast<char*>(buffer.data()), static_cast<std::streamsize>(size));
    return buffer;
}

std::vector<u8> load_spv_asset(const char* filename) {
    const std::array<std::string, 3> candidates = {
        std::string("shaders/") + filename,
        std::string("../shaders/") + filename,
        std::string(filename),
    };
    for (const auto& path : candidates) {
        if (std::filesystem::exists(path)) {
            auto data = read_binary_file(path);
            if (!data.empty()) return data;
        }
    }
    return {};
}

} // namespace

VulkanRenderer::VulkanRenderer() = default;

VulkanRenderer::~VulkanRenderer() {
    shutdown();
}

bool VulkanRenderer::init(SDL_Window* window, const RenderSettings& settings) {
    window_   = window;
    settings_ = settings;
    width_    = settings.width;
    height_   = settings.height;
    vsync_    = settings.vsync;

    if (!create_instance(window)) return false;
    if (!SDL_Vulkan_CreateSurface(window, instance_, &surface_)) {
        std::fprintf(stderr, "[Vulkan] SDL_Vulkan_CreateSurface failed: %s\n", SDL_GetError());
        return false;
    }
    if (!pick_physical_device()) return false;
    if (!create_logical_device()) return false;
    if (!create_swapchain()) return false;
    if (!create_render_pass()) return false;
    if (!create_depth_resources()) return false;
    if (!create_framebuffers()) return false;
    if (!create_command_pool_and_buffers()) return false;
    if (!create_sync_objects()) return false;
    if (!create_descriptor_resources()) return false;
    if (!create_graphics_pipeline()) return false;
    if (!create_default_resources()) return false;

    std::printf("[Vulkan] Initialized (%ux%u)\n", width_, height_);
    return true;
}

void VulkanRenderer::shutdown() {
    if (device_ != VK_NULL_HANDLE) {
        vkDeviceWaitIdle(device_);
    }

    // Destroy user/default meshes
    for (auto& [id, mesh] : meshes_) {
        if (mesh.vb != VK_NULL_HANDLE) vkDestroyBuffer(device_, mesh.vb, nullptr);
        if (mesh.vb_mem != VK_NULL_HANDLE) vkFreeMemory(device_, mesh.vb_mem, nullptr);
        if (mesh.ib != VK_NULL_HANDLE) vkDestroyBuffer(device_, mesh.ib, nullptr);
        if (mesh.ib_mem != VK_NULL_HANDLE) vkFreeMemory(device_, mesh.ib_mem, nullptr);
    }
    meshes_.clear();
    unit_quad_mesh_ = {};

    // Destroy textures
    for (auto& [id, tex] : textures_) {
        if (tex.view != VK_NULL_HANDLE) vkDestroyImageView(device_, tex.view, nullptr);
        if (tex.image != VK_NULL_HANDLE) vkDestroyImage(device_, tex.image, nullptr);
        if (tex.memory != VK_NULL_HANDLE) vkFreeMemory(device_, tex.memory, nullptr);
    }
    textures_.clear();
    default_white_tex_ = {};

    cleanup_swapchain();

    if (device_ != VK_NULL_HANDLE) {
        if (sampler_linear_ != VK_NULL_HANDLE) {
            vkDestroySampler(device_, sampler_linear_, nullptr);
            sampler_linear_ = VK_NULL_HANDLE;
        }
        if (descriptor_pool_ != VK_NULL_HANDLE) {
            vkDestroyDescriptorPool(device_, descriptor_pool_, nullptr);
            descriptor_pool_ = VK_NULL_HANDLE;
        }
        if (descriptor_layout_ != VK_NULL_HANDLE) {
            vkDestroyDescriptorSetLayout(device_, descriptor_layout_, nullptr);
            descriptor_layout_ = VK_NULL_HANDLE;
        }
        if (graphics_pipeline_ != VK_NULL_HANDLE) {
            vkDestroyPipeline(device_, graphics_pipeline_, nullptr);
            graphics_pipeline_ = VK_NULL_HANDLE;
        }
        if (pipeline_layout_ != VK_NULL_HANDLE) {
            vkDestroyPipelineLayout(device_, pipeline_layout_, nullptr);
            pipeline_layout_ = VK_NULL_HANDLE;
        }
        if (render_pass_ != VK_NULL_HANDLE) {
            vkDestroyRenderPass(device_, render_pass_, nullptr);
            render_pass_ = VK_NULL_HANDLE;
        }

        for (size_t i = 0; i < image_available_semaphores_.size(); ++i) {
            vkDestroySemaphore(device_, image_available_semaphores_[i], nullptr);
            vkDestroySemaphore(device_, render_finished_semaphores_[i], nullptr);
            vkDestroyFence(device_, in_flight_fences_[i], nullptr);
        }
        image_available_semaphores_.clear();
        render_finished_semaphores_.clear();
        in_flight_fences_.clear();

        if (command_pool_ != VK_NULL_HANDLE) {
            vkDestroyCommandPool(device_, command_pool_, nullptr);
            command_pool_ = VK_NULL_HANDLE;
        }

        vkDestroyDevice(device_, nullptr);
        device_ = VK_NULL_HANDLE;
    }

    if (surface_ != VK_NULL_HANDLE && instance_ != VK_NULL_HANDLE) {
        vkDestroySurfaceKHR(instance_, surface_, nullptr);
        surface_ = VK_NULL_HANDLE;
    }
    if (instance_ != VK_NULL_HANDLE) {
        vkDestroyInstance(instance_, nullptr);
        instance_ = VK_NULL_HANDLE;
    }

    std::printf("[Vulkan] Shut down\n");
}

// ── Initialization ───────────────────────────────────────────────────────────

bool VulkanRenderer::create_instance(SDL_Window* window) {
    u32 ext_count = 0;
    if (!SDL_Vulkan_GetInstanceExtensions(window, &ext_count, nullptr)) {
        std::fprintf(stderr, "[Vulkan] SDL_Vulkan_GetInstanceExtensions failed: %s\n", SDL_GetError());
        return false;
    }
    std::vector<const char*> extensions(ext_count);
    SDL_Vulkan_GetInstanceExtensions(window, &ext_count, extensions.data());

    VkApplicationInfo app_info{};
    app_info.sType              = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    app_info.pApplicationName   = "hotd3-re";
    app_info.applicationVersion = VK_MAKE_VERSION(0, 1, 0);
    app_info.pEngineName        = "hotd3-re";
    app_info.engineVersion      = VK_MAKE_VERSION(0, 1, 0);
    app_info.apiVersion         = VK_API_VERSION_1_1;

    VkInstanceCreateInfo ci{};
    ci.sType                   = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    ci.pApplicationInfo        = &app_info;
    ci.enabledExtensionCount   = static_cast<u32>(extensions.size());
    ci.ppEnabledExtensionNames = extensions.data();

#ifdef _DEBUG
    const char* validation_layer = "VK_LAYER_KHRONOS_validation";
    u32 layer_count = 0;
    vkEnumerateInstanceLayerProperties(&layer_count, nullptr);
    std::vector<VkLayerProperties> available_layers(layer_count);
    vkEnumerateInstanceLayerProperties(&layer_count, available_layers.data());
    for (const auto& layer : available_layers) {
        if (std::strcmp(layer.layerName, validation_layer) == 0) {
            ci.enabledLayerCount   = 1;
            ci.ppEnabledLayerNames = &validation_layer;
            break;
        }
    }
#endif

    VkResult res = vkCreateInstance(&ci, nullptr, &instance_);
    if (res != VK_SUCCESS) {
        std::fprintf(stderr, "[Vulkan] vkCreateInstance failed (%d)\n", res);
        return false;
    }
    return true;
}

bool VulkanRenderer::pick_physical_device() {
    u32 device_count = 0;
    vkEnumeratePhysicalDevices(instance_, &device_count, nullptr);
    if (device_count == 0) {
        std::fprintf(stderr, "[Vulkan] No Vulkan-capable GPUs found\n");
        return false;
    }

    std::vector<VkPhysicalDevice> devices(device_count);
    vkEnumeratePhysicalDevices(instance_, &device_count, devices.data());

    for (VkPhysicalDevice dev : devices) {
        u32 qf_count = 0;
        vkGetPhysicalDeviceQueueFamilyProperties(dev, &qf_count, nullptr);
        std::vector<VkQueueFamilyProperties> qfs(qf_count);
        vkGetPhysicalDeviceQueueFamilyProperties(dev, &qf_count, qfs.data());

        bool has_gfx = false;
        bool has_present = false;
        u32 gfx_idx = 0;
        u32 present_idx = 0;

        for (u32 i = 0; i < qf_count; ++i) {
            if (qfs[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) {
                has_gfx = true;
                gfx_idx = i;
            }
            VkBool32 present_support = VK_FALSE;
            vkGetPhysicalDeviceSurfaceSupportKHR(dev, i, surface_, &present_support);
            if (present_support) {
                has_present = true;
                present_idx = i;
            }
            if (has_gfx && has_present) break;
        }

        if (!has_gfx || !has_present) continue;

        u32 ext_count = 0;
        vkEnumerateDeviceExtensionProperties(dev, nullptr, &ext_count, nullptr);
        std::vector<VkExtensionProperties> exts(ext_count);
        vkEnumerateDeviceExtensionProperties(dev, nullptr, &ext_count, exts.data());

        bool has_swapchain = false;
        for (const auto& ext : exts) {
            if (std::strcmp(ext.extensionName, VK_KHR_SWAPCHAIN_EXTENSION_NAME) == 0) {
                has_swapchain = true;
                break;
            }
        }
        if (!has_swapchain) continue;

        VkPhysicalDeviceProperties props{};
        vkGetPhysicalDeviceProperties(dev, &props);

        physical_device_ = dev;
        graphics_family_ = gfx_idx;
        present_family_  = present_idx;

        if (props.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU) {
            std::printf("[Vulkan] Selected GPU: %s\n", props.deviceName);
            return true;
        }
    }

    if (physical_device_ != VK_NULL_HANDLE) {
        VkPhysicalDeviceProperties props{};
        vkGetPhysicalDeviceProperties(physical_device_, &props);
        std::printf("[Vulkan] Selected GPU: %s\n", props.deviceName);
        return true;
    }

    std::fprintf(stderr, "[Vulkan] Failed to find suitable GPU\n");
    return false;
}

bool VulkanRenderer::create_logical_device() {
    std::set<u32> unique_families = {graphics_family_, present_family_};
    std::vector<VkDeviceQueueCreateInfo> queue_cis;
    f32 priority = 1.0f;

    for (u32 family : unique_families) {
        VkDeviceQueueCreateInfo qci{};
        qci.sType            = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        qci.queueFamilyIndex = family;
        qci.queueCount       = 1;
        qci.pQueuePriorities = &priority;
        queue_cis.push_back(qci);
    }

    const char* device_extensions[] = {VK_KHR_SWAPCHAIN_EXTENSION_NAME};
    VkPhysicalDeviceFeatures features{};

    VkDeviceCreateInfo dci{};
    dci.sType                   = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    dci.queueCreateInfoCount    = static_cast<u32>(queue_cis.size());
    dci.pQueueCreateInfos       = queue_cis.data();
    dci.enabledExtensionCount   = 1;
    dci.ppEnabledExtensionNames = device_extensions;
    dci.pEnabledFeatures        = &features;

    VkResult res = vkCreateDevice(physical_device_, &dci, nullptr, &device_);
    if (res != VK_SUCCESS) {
        std::fprintf(stderr, "[Vulkan] vkCreateDevice failed (%d)\n", res);
        return false;
    }

    vkGetDeviceQueue(device_, graphics_family_, 0, &graphics_queue_);
    vkGetDeviceQueue(device_, present_family_, 0, &present_queue_);
    return true;
}

bool VulkanRenderer::create_swapchain() {
    VkSurfaceCapabilitiesKHR caps{};
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physical_device_, surface_, &caps);

    u32 format_count = 0;
    vkGetPhysicalDeviceSurfaceFormatsKHR(physical_device_, surface_, &format_count, nullptr);
    std::vector<VkSurfaceFormatKHR> formats(format_count);
    vkGetPhysicalDeviceSurfaceFormatsKHR(physical_device_, surface_, &format_count, formats.data());

    VkSurfaceFormatKHR chosen_format = formats[0];
    for (const auto& f : formats) {
        if (f.format == VK_FORMAT_B8G8R8A8_UNORM &&
            f.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
            chosen_format = f;
            break;
        }
    }

    u32 pm_count = 0;
    vkGetPhysicalDeviceSurfacePresentModesKHR(physical_device_, surface_, &pm_count, nullptr);
    std::vector<VkPresentModeKHR> present_modes(pm_count);
    vkGetPhysicalDeviceSurfacePresentModesKHR(physical_device_, surface_, &pm_count, present_modes.data());

    VkPresentModeKHR chosen_pm = VK_PRESENT_MODE_FIFO_KHR;
    if (!vsync_) {
        for (VkPresentModeKHR pm : present_modes) {
            if (pm == VK_PRESENT_MODE_MAILBOX_KHR) {
                chosen_pm = pm;
                break;
            }
            if (pm == VK_PRESENT_MODE_IMMEDIATE_KHR) {
                chosen_pm = pm;
            }
        }
    }

    VkExtent2D extent{};
    if (caps.currentExtent.width != UINT32_MAX) {
        extent = caps.currentExtent;
    } else {
        extent.width  = std::clamp(width_, caps.minImageExtent.width, caps.maxImageExtent.width);
        extent.height = std::clamp(height_, caps.minImageExtent.height, caps.maxImageExtent.height);
    }

    u32 image_count = caps.minImageCount + 1;
    if (caps.maxImageCount > 0 && image_count > caps.maxImageCount) {
        image_count = caps.maxImageCount;
    }

    VkSwapchainCreateInfoKHR sci{};
    sci.sType            = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    sci.surface          = surface_;
    sci.minImageCount    = image_count;
    sci.imageFormat      = chosen_format.format;
    sci.imageColorSpace  = chosen_format.colorSpace;
    sci.imageExtent      = extent;
    sci.imageArrayLayers = 1;
    sci.imageUsage       = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

    u32 queue_indices[] = {graphics_family_, present_family_};
    if (graphics_family_ != present_family_) {
        sci.imageSharingMode      = VK_SHARING_MODE_CONCURRENT;
        sci.queueFamilyIndexCount = 2;
        sci.pQueueFamilyIndices   = queue_indices;
    } else {
        sci.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    }

    sci.preTransform   = caps.currentTransform;
    sci.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    sci.presentMode    = chosen_pm;
    sci.clipped        = VK_TRUE;

    if (vkCreateSwapchainKHR(device_, &sci, nullptr, &swapchain_) != VK_SUCCESS) {
        std::fprintf(stderr, "[Vulkan] vkCreateSwapchainKHR failed\n");
        return false;
    }

    swapchain_format_ = chosen_format.format;
    swapchain_extent_ = extent;
    width_  = extent.width;
    height_ = extent.height;

    vkGetSwapchainImagesKHR(device_, swapchain_, &image_count, nullptr);
    swapchain_images_.resize(image_count);
    vkGetSwapchainImagesKHR(device_, swapchain_, &image_count, swapchain_images_.data());

    swapchain_views_.resize(image_count);
    for (size_t i = 0; i < image_count; ++i) {
        VkImageViewCreateInfo ivci{};
        ivci.sType                           = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        ivci.image                           = swapchain_images_[i];
        ivci.viewType                        = VK_IMAGE_VIEW_TYPE_2D;
        ivci.format                          = swapchain_format_;
        ivci.subresourceRange.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
        ivci.subresourceRange.baseMipLevel   = 0;
        ivci.subresourceRange.levelCount     = 1;
        ivci.subresourceRange.baseArrayLayer = 0;
        ivci.subresourceRange.layerCount     = 1;

        if (vkCreateImageView(device_, &ivci, nullptr, &swapchain_views_[i]) != VK_SUCCESS) {
            return false;
        }
    }

    return true;
}

bool VulkanRenderer::create_render_pass() {
    depth_format_ = find_depth_format();

    VkAttachmentDescription color_att{};
    color_att.format         = swapchain_format_;
    color_att.samples        = VK_SAMPLE_COUNT_1_BIT;
    color_att.loadOp         = VK_ATTACHMENT_LOAD_OP_CLEAR;
    color_att.storeOp        = VK_ATTACHMENT_STORE_OP_STORE;
    color_att.stencilLoadOp  = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    color_att.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    color_att.initialLayout  = VK_IMAGE_LAYOUT_UNDEFINED;
    color_att.finalLayout    = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

    VkAttachmentDescription depth_att{};
    depth_att.format         = depth_format_;
    depth_att.samples        = VK_SAMPLE_COUNT_1_BIT;
    depth_att.loadOp         = VK_ATTACHMENT_LOAD_OP_CLEAR;
    depth_att.storeOp        = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    depth_att.stencilLoadOp  = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    depth_att.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    depth_att.initialLayout  = VK_IMAGE_LAYOUT_UNDEFINED;
    depth_att.finalLayout    = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    VkAttachmentReference color_ref{0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};
    VkAttachmentReference depth_ref{1, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL};

    VkSubpassDescription subpass{};
    subpass.pipelineBindPoint       = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount    = 1;
    subpass.pColorAttachments       = &color_ref;
    subpass.pDepthStencilAttachment = &depth_ref;

    VkSubpassDependency dep{};
    dep.srcSubpass    = VK_SUBPASS_EXTERNAL;
    dep.dstSubpass    = 0;
    dep.srcStageMask  = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT |
                        VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
    dep.dstStageMask  = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT |
                        VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
    dep.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT |
                        VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

    std::array<VkAttachmentDescription, 2> attachments = {color_att, depth_att};
    VkRenderPassCreateInfo rpci{};
    rpci.sType           = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    rpci.attachmentCount = static_cast<u32>(attachments.size());
    rpci.pAttachments    = attachments.data();
    rpci.subpassCount    = 1;
    rpci.pSubpasses      = &subpass;
    rpci.dependencyCount = 1;
    rpci.pDependencies   = &dep;

    return vkCreateRenderPass(device_, &rpci, nullptr, &render_pass_) == VK_SUCCESS;
}

bool VulkanRenderer::create_depth_resources() {
    VkImageCreateInfo ici{};
    ici.sType         = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    ici.imageType     = VK_IMAGE_TYPE_2D;
    ici.extent        = {swapchain_extent_.width, swapchain_extent_.height, 1};
    ici.mipLevels     = 1;
    ici.arrayLayers   = 1;
    ici.format        = depth_format_;
    ici.tiling        = VK_IMAGE_TILING_OPTIMAL;
    ici.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    ici.usage         = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
    ici.samples       = VK_SAMPLE_COUNT_1_BIT;
    ici.sharingMode   = VK_SHARING_MODE_EXCLUSIVE;

    if (vkCreateImage(device_, &ici, nullptr, &depth_image_) != VK_SUCCESS) return false;

    VkMemoryRequirements mem_reqs{};
    vkGetImageMemoryRequirements(device_, depth_image_, &mem_reqs);

    VkMemoryAllocateInfo mai{};
    mai.sType           = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    mai.allocationSize  = mem_reqs.size;
    mai.memoryTypeIndex = find_memory_type(mem_reqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

    if (vkAllocateMemory(device_, &mai, nullptr, &depth_memory_) != VK_SUCCESS) return false;
    vkBindImageMemory(device_, depth_image_, depth_memory_, 0);

    VkImageViewCreateInfo ivci{};
    ivci.sType                           = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    ivci.image                           = depth_image_;
    ivci.viewType                        = VK_IMAGE_VIEW_TYPE_2D;
    ivci.format                          = depth_format_;
    ivci.subresourceRange.aspectMask     = VK_IMAGE_ASPECT_DEPTH_BIT;
    ivci.subresourceRange.baseMipLevel   = 0;
    ivci.subresourceRange.levelCount     = 1;
    ivci.subresourceRange.baseArrayLayer = 0;
    ivci.subresourceRange.layerCount     = 1;

    return vkCreateImageView(device_, &ivci, nullptr, &depth_view_) == VK_SUCCESS;
}

bool VulkanRenderer::create_framebuffers() {
    framebuffers_.resize(swapchain_views_.size());
    for (size_t i = 0; i < swapchain_views_.size(); ++i) {
        std::array<VkImageView, 2> attachments = {swapchain_views_[i], depth_view_};

        VkFramebufferCreateInfo fbci{};
        fbci.sType           = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        fbci.renderPass      = render_pass_;
        fbci.attachmentCount = static_cast<u32>(attachments.size());
        fbci.pAttachments    = attachments.data();
        fbci.width           = swapchain_extent_.width;
        fbci.height          = swapchain_extent_.height;
        fbci.layers          = 1;

        if (vkCreateFramebuffer(device_, &fbci, nullptr, &framebuffers_[i]) != VK_SUCCESS) {
            return false;
        }
    }
    return true;
}

bool VulkanRenderer::create_command_pool_and_buffers() {
    VkCommandPoolCreateInfo cpci{};
    cpci.sType            = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    cpci.flags            = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    cpci.queueFamilyIndex = graphics_family_;

    if (vkCreateCommandPool(device_, &cpci, nullptr, &command_pool_) != VK_SUCCESS) {
        return false;
    }

    command_buffers_.resize(MAX_FRAMES_IN_FLIGHT);
    VkCommandBufferAllocateInfo cbai{};
    cbai.sType              = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    cbai.commandPool        = command_pool_;
    cbai.level              = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    cbai.commandBufferCount = MAX_FRAMES_IN_FLIGHT;

    return vkAllocateCommandBuffers(device_, &cbai, command_buffers_.data()) == VK_SUCCESS;
}

bool VulkanRenderer::create_sync_objects() {
    image_available_semaphores_.resize(MAX_FRAMES_IN_FLIGHT);
    render_finished_semaphores_.resize(MAX_FRAMES_IN_FLIGHT);
    in_flight_fences_.resize(MAX_FRAMES_IN_FLIGHT);

    VkSemaphoreCreateInfo sci{};
    sci.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

    VkFenceCreateInfo fci{};
    fci.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fci.flags = VK_FENCE_CREATE_SIGNALED_BIT;

    for (u32 i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
        if (vkCreateSemaphore(device_, &sci, nullptr, &image_available_semaphores_[i]) != VK_SUCCESS ||
            vkCreateSemaphore(device_, &sci, nullptr, &render_finished_semaphores_[i]) != VK_SUCCESS ||
            vkCreateFence(device_, &fci, nullptr, &in_flight_fences_[i]) != VK_SUCCESS) {
            return false;
        }
    }
    return true;
}

bool VulkanRenderer::create_descriptor_resources() {
    VkDescriptorSetLayoutBinding sampler_binding{};
    sampler_binding.binding         = 0;
    sampler_binding.descriptorType  = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    sampler_binding.descriptorCount = 1;
    sampler_binding.stageFlags      = VK_SHADER_STAGE_FRAGMENT_BIT;

    VkDescriptorSetLayoutCreateInfo dslci{};
    dslci.sType        = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    dslci.bindingCount = 1;
    dslci.pBindings    = &sampler_binding;

    if (vkCreateDescriptorSetLayout(device_, &dslci, nullptr, &descriptor_layout_) != VK_SUCCESS) {
        return false;
    }

    VkDescriptorPoolSize pool_size{};
    pool_size.type            = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    pool_size.descriptorCount = 1024;

    VkDescriptorPoolCreateInfo dpci{};
    dpci.sType         = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    dpci.flags         = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
    dpci.maxSets       = 1024;
    dpci.poolSizeCount = 1;
    dpci.pPoolSizes    = &pool_size;

    if (vkCreateDescriptorPool(device_, &dpci, nullptr, &descriptor_pool_) != VK_SUCCESS) {
        return false;
    }

    VkSamplerCreateInfo sci{};
    sci.sType        = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    sci.magFilter    = VK_FILTER_LINEAR;
    sci.minFilter    = VK_FILTER_LINEAR;
    sci.mipmapMode   = VK_SAMPLER_MIPMAP_MODE_LINEAR;
    sci.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    sci.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    sci.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    sci.maxAnisotropy = 1.0f;

    return vkCreateSampler(device_, &sci, nullptr, &sampler_linear_) == VK_SUCCESS;
}

bool VulkanRenderer::create_graphics_pipeline() {
    VkPushConstantRange push_range{};
    push_range.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
    push_range.offset     = 0;
    push_range.size       = sizeof(PushConstants);

    VkPipelineLayoutCreateInfo plci{};
    plci.sType                  = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    plci.setLayoutCount         = 1;
    plci.pSetLayouts            = &descriptor_layout_;
    plci.pushConstantRangeCount = 1;
    plci.pPushConstantRanges    = &push_range;

    if (vkCreatePipelineLayout(device_, &plci, nullptr, &pipeline_layout_) != VK_SUCCESS) {
        return false;
    }

    auto vert_code = load_spv_asset("basic.vert.spv");
    auto frag_code = load_spv_asset("basic.frag.spv");
    if (vert_code.empty() || frag_code.empty()) {
        std::printf("[Vulkan] SPIR-V shaders not found; continuing with clear-pass only\n");
        return true;
    }

    VkShaderModule vert_mod = create_shader_module(vert_code);
    VkShaderModule frag_mod = create_shader_module(frag_code);
    if (vert_mod == VK_NULL_HANDLE || frag_mod == VK_NULL_HANDLE) {
        if (vert_mod != VK_NULL_HANDLE) vkDestroyShaderModule(device_, vert_mod, nullptr);
        if (frag_mod != VK_NULL_HANDLE) vkDestroyShaderModule(device_, frag_mod, nullptr);
        return false;
    }

    VkPipelineShaderStageCreateInfo stages[2]{};
    stages[0].sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    stages[0].stage  = VK_SHADER_STAGE_VERTEX_BIT;
    stages[0].module = vert_mod;
    stages[0].pName  = "main";

    stages[1].sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    stages[1].stage  = VK_SHADER_STAGE_FRAGMENT_BIT;
    stages[1].module = frag_mod;
    stages[1].pName  = "main";

    VkVertexInputBindingDescription binding{};
    binding.binding   = 0;
    binding.stride    = sizeof(Vertex);
    binding.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

    std::array<VkVertexInputAttributeDescription, 4> attrs{};
    attrs[0] = {0, 0, VK_FORMAT_R32G32B32_SFLOAT,    static_cast<u32>(offsetof(Vertex, position))};
    attrs[1] = {1, 0, VK_FORMAT_R32G32B32_SFLOAT,    static_cast<u32>(offsetof(Vertex, normal))};
    attrs[2] = {2, 0, VK_FORMAT_R32G32_SFLOAT,       static_cast<u32>(offsetof(Vertex, texcoord))};
    attrs[3] = {3, 0, VK_FORMAT_R32G32B32A32_SFLOAT, static_cast<u32>(offsetof(Vertex, color))};

    VkPipelineVertexInputStateCreateInfo vertex_input{};
    vertex_input.sType                           = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vertex_input.vertexBindingDescriptionCount   = 1;
    vertex_input.pVertexBindingDescriptions      = &binding;
    vertex_input.vertexAttributeDescriptionCount = static_cast<u32>(attrs.size());
    vertex_input.pVertexAttributeDescriptions    = attrs.data();

    VkPipelineInputAssemblyStateCreateInfo input_assembly{};
    input_assembly.sType    = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    input_assembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

    VkPipelineViewportStateCreateInfo viewport_state{};
    viewport_state.sType         = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewport_state.viewportCount = 1;
    viewport_state.scissorCount  = 1;

    VkPipelineRasterizationStateCreateInfo rasterizer{};
    rasterizer.sType       = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
    rasterizer.cullMode    = VK_CULL_MODE_BACK_BIT;
    rasterizer.frontFace   = VK_FRONT_FACE_CLOCKWISE;
    rasterizer.lineWidth   = 1.0f;

    VkPipelineMultisampleStateCreateInfo multisampling{};
    multisampling.sType                = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

    VkPipelineDepthStencilStateCreateInfo depth_stencil{};
    depth_stencil.sType            = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    depth_stencil.depthTestEnable  = VK_TRUE;
    depth_stencil.depthWriteEnable = VK_TRUE;
    depth_stencil.depthCompareOp   = VK_COMPARE_OP_LESS;

    VkPipelineColorBlendAttachmentState blend_att{};
    blend_att.blendEnable         = VK_TRUE;
    blend_att.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
    blend_att.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    blend_att.colorBlendOp        = VK_BLEND_OP_ADD;
    blend_att.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
    blend_att.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
    blend_att.alphaBlendOp        = VK_BLEND_OP_ADD;
    blend_att.colorWriteMask      = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
                                    VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;

    VkPipelineColorBlendStateCreateInfo color_blending{};
    color_blending.sType           = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    color_blending.attachmentCount = 1;
    color_blending.pAttachments    = &blend_att;

    std::array<VkDynamicState, 2> dyn_states = {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};
    VkPipelineDynamicStateCreateInfo dynamic_state{};
    dynamic_state.sType             = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    dynamic_state.dynamicStateCount = static_cast<u32>(dyn_states.size());
    dynamic_state.pDynamicStates    = dyn_states.data();

    VkGraphicsPipelineCreateInfo gpci{};
    gpci.sType               = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    gpci.stageCount          = 2;
    gpci.pStages             = stages;
    gpci.pVertexInputState   = &vertex_input;
    gpci.pInputAssemblyState = &input_assembly;
    gpci.pViewportState      = &viewport_state;
    gpci.pRasterizationState = &rasterizer;
    gpci.pMultisampleState   = &multisampling;
    gpci.pDepthStencilState  = &depth_stencil;
    gpci.pColorBlendState    = &color_blending;
    gpci.pDynamicState       = &dynamic_state;
    gpci.layout              = pipeline_layout_;
    gpci.renderPass          = render_pass_;
    gpci.subpass             = 0;

    VkResult res = vkCreateGraphicsPipelines(device_, VK_NULL_HANDLE, 1, &gpci, nullptr, &graphics_pipeline_);
    vkDestroyShaderModule(device_, vert_mod, nullptr);
    vkDestroyShaderModule(device_, frag_mod, nullptr);

    if (res != VK_SUCCESS) {
        std::fprintf(stderr, "[Vulkan] vkCreateGraphicsPipelines failed (%d)\n", res);
        return false;
    }

    std::printf("[Vulkan] Graphics pipeline created\n");
    return true;
}

bool VulkanRenderer::create_default_resources() {
    const u8 white_pixel[4] = {255, 255, 255, 255};
    default_white_tex_ = load_texture(white_pixel, 1, 1, TextureFormat::RGBA8);
    if (!default_white_tex_.valid()) return false;

    MeshData quad;
    quad.vertices = {
        {{0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 1.0f}, {0.0f, 0.0f}, Color::white()},
        {{1.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 1.0f}, {1.0f, 0.0f}, Color::white()},
        {{1.0f, 1.0f, 0.0f}, {0.0f, 0.0f, 1.0f}, {1.0f, 1.0f}, Color::white()},
        {{0.0f, 1.0f, 0.0f}, {0.0f, 0.0f, 1.0f}, {0.0f, 1.0f}, Color::white()},
    };
    quad.indices = {0, 1, 2, 0, 2, 3};
    unit_quad_mesh_ = upload_mesh(quad);
    return unit_quad_mesh_.valid();
}

void VulkanRenderer::cleanup_swapchain() {
    if (device_ == VK_NULL_HANDLE) return;

    for (VkFramebuffer fb : framebuffers_) {
        vkDestroyFramebuffer(device_, fb, nullptr);
    }
    framebuffers_.clear();

    if (depth_view_ != VK_NULL_HANDLE) {
        vkDestroyImageView(device_, depth_view_, nullptr);
        depth_view_ = VK_NULL_HANDLE;
    }
    if (depth_image_ != VK_NULL_HANDLE) {
        vkDestroyImage(device_, depth_image_, nullptr);
        depth_image_ = VK_NULL_HANDLE;
    }
    if (depth_memory_ != VK_NULL_HANDLE) {
        vkFreeMemory(device_, depth_memory_, nullptr);
        depth_memory_ = VK_NULL_HANDLE;
    }

    for (VkImageView iv : swapchain_views_) {
        vkDestroyImageView(device_, iv, nullptr);
    }
    swapchain_views_.clear();

    if (swapchain_ != VK_NULL_HANDLE) {
        vkDestroySwapchainKHR(device_, swapchain_, nullptr);
        swapchain_ = VK_NULL_HANDLE;
    }
}

bool VulkanRenderer::recreate_swapchain() {
    if (width_ == 0 || height_ == 0) return false;
    vkDeviceWaitIdle(device_);
    cleanup_swapchain();
    if (!create_swapchain()) return false;
    if (!create_depth_resources()) return false;
    if (!create_framebuffers()) return false;
    framebuffer_resized_ = false;
    return true;
}

// ── Frame ────────────────────────────────────────────────────────────────────

void VulkanRenderer::begin_frame(const Color& clear_color) {
    if (device_ == VK_NULL_HANDLE || swapchain_ == VK_NULL_HANDLE) return;

    vkWaitForFences(device_, 1, &in_flight_fences_[current_frame_], VK_TRUE, UINT64_MAX);

    VkResult res = vkAcquireNextImageKHR(
        device_, swapchain_, UINT64_MAX,
        image_available_semaphores_[current_frame_], VK_NULL_HANDLE,
        &current_image_index_
    );

    if (res == VK_ERROR_OUT_OF_DATE_KHR) {
        recreate_swapchain();
        return;
    }
    if (res != VK_SUCCESS && res != VK_SUBOPTIMAL_KHR) {
        return;
    }

    vkResetFences(device_, 1, &in_flight_fences_[current_frame_]);

    VkCommandBuffer cmd = command_buffers_[current_frame_];
    vkResetCommandBuffer(cmd, 0);

    VkCommandBufferBeginInfo bi{};
    bi.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    if (vkBeginCommandBuffer(cmd, &bi) != VK_SUCCESS) return;

    std::array<VkClearValue, 2> clears{};
    clears[0].color = {{clear_color.r, clear_color.g, clear_color.b, clear_color.a}};
    clears[1].depthStencil = {1.0f, 0};

    VkRenderPassBeginInfo rpbi{};
    rpbi.sType             = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    rpbi.renderPass        = render_pass_;
    rpbi.framebuffer       = framebuffers_[current_image_index_];
    rpbi.renderArea.offset = {0, 0};
    rpbi.renderArea.extent = swapchain_extent_;
    rpbi.clearValueCount   = static_cast<u32>(clears.size());
    rpbi.pClearValues      = clears.data();

    vkCmdBeginRenderPass(cmd, &rpbi, VK_SUBPASS_CONTENTS_INLINE);

    VkViewport vp{};
    vp.x        = 0.0f;
    vp.y        = 0.0f;
    vp.width    = static_cast<f32>(swapchain_extent_.width);
    vp.height   = static_cast<f32>(swapchain_extent_.height);
    vp.minDepth = 0.0f;
    vp.maxDepth = 1.0f;
    vkCmdSetViewport(cmd, 0, 1, &vp);

    VkRect2D scissor{{0, 0}, swapchain_extent_};
    vkCmdSetScissor(cmd, 0, 1, &scissor);

    if (graphics_pipeline_ != VK_NULL_HANDLE) {
        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, graphics_pipeline_);
    }

    frame_active_ = true;
}

void VulkanRenderer::end_frame() {
    if (!frame_active_) return;
    frame_active_ = false;

    VkCommandBuffer cmd = command_buffers_[current_frame_];
    vkCmdEndRenderPass(cmd);
    if (vkEndCommandBuffer(cmd) != VK_SUCCESS) return;

    VkPipelineStageFlags wait_stage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    VkSubmitInfo si{};
    si.sType                = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    si.waitSemaphoreCount   = 1;
    si.pWaitSemaphores      = &image_available_semaphores_[current_frame_];
    si.pWaitDstStageMask    = &wait_stage;
    si.commandBufferCount   = 1;
    si.pCommandBuffers      = &cmd;
    si.signalSemaphoreCount = 1;
    si.pSignalSemaphores    = &render_finished_semaphores_[current_frame_];

    if (vkQueueSubmit(graphics_queue_, 1, &si, in_flight_fences_[current_frame_]) != VK_SUCCESS) {
        return;
    }

    VkPresentInfoKHR pi{};
    pi.sType              = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    pi.waitSemaphoreCount = 1;
    pi.pWaitSemaphores    = &render_finished_semaphores_[current_frame_];
    pi.swapchainCount     = 1;
    pi.pSwapchains        = &swapchain_;
    pi.pImageIndices      = &current_image_index_;

    VkResult res = vkQueuePresentKHR(present_queue_, &pi);
    if (res == VK_ERROR_OUT_OF_DATE_KHR || res == VK_SUBOPTIMAL_KHR || framebuffer_resized_) {
        recreate_swapchain();
    }

    current_frame_ = (current_frame_ + 1) % MAX_FRAMES_IN_FLIGHT;
}

// ── 3D & 2D Rendering ────────────────────────────────────────────────────────

MeshHandle VulkanRenderer::upload_mesh(const MeshData& data) {
    if (device_ == VK_NULL_HANDLE || data.vertices.empty()) return {};

    VkMesh mesh{};
    const VkDeviceSize vb_size = static_cast<VkDeviceSize>(data.vertices.size()) * sizeof(Vertex);

    VkBuffer staging_vb = VK_NULL_HANDLE;
    VkDeviceMemory staging_vb_mem = VK_NULL_HANDLE;
    if (!create_buffer(vb_size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                       VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                       staging_vb, staging_vb_mem)) {
        return {};
    }

    void* mapped = nullptr;
    vkMapMemory(device_, staging_vb_mem, 0, vb_size, 0, &mapped);
    std::memcpy(mapped, data.vertices.data(), static_cast<size_t>(vb_size));
    vkUnmapMemory(device_, staging_vb_mem);

    if (!create_buffer(vb_size,
                       VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
                       VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, mesh.vb, mesh.vb_mem)) {
        vkDestroyBuffer(device_, staging_vb, nullptr);
        vkFreeMemory(device_, staging_vb_mem, nullptr);
        return {};
    }
    copy_buffer(staging_vb, mesh.vb, vb_size);
    vkDestroyBuffer(device_, staging_vb, nullptr);
    vkFreeMemory(device_, staging_vb_mem, nullptr);

    if (!data.indices.empty()) {
        const VkDeviceSize ib_size = static_cast<VkDeviceSize>(data.indices.size()) * sizeof(u32);
        VkBuffer staging_ib = VK_NULL_HANDLE;
        VkDeviceMemory staging_ib_mem = VK_NULL_HANDLE;
        if (!create_buffer(ib_size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                           VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                           staging_ib, staging_ib_mem)) {
            vkDestroyBuffer(device_, mesh.vb, nullptr);
            vkFreeMemory(device_, mesh.vb_mem, nullptr);
            return {};
        }

        vkMapMemory(device_, staging_ib_mem, 0, ib_size, 0, &mapped);
        std::memcpy(mapped, data.indices.data(), static_cast<size_t>(ib_size));
        vkUnmapMemory(device_, staging_ib_mem);

        if (!create_buffer(ib_size,
                           VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
                           VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, mesh.ib, mesh.ib_mem)) {
            vkDestroyBuffer(device_, staging_ib, nullptr);
            vkFreeMemory(device_, staging_ib_mem, nullptr);
            vkDestroyBuffer(device_, mesh.vb, nullptr);
            vkFreeMemory(device_, mesh.vb_mem, nullptr);
            return {};
        }
        copy_buffer(staging_ib, mesh.ib, ib_size);
        vkDestroyBuffer(device_, staging_ib, nullptr);
        vkFreeMemory(device_, staging_ib_mem, nullptr);
        mesh.index_count = static_cast<u32>(data.indices.size());
    }

    MeshHandle h{};
    h.id = next_mesh_id_++;
    meshes_[h.id] = mesh;
    return h;
}

void VulkanRenderer::destroy_mesh(MeshHandle handle) {
    if (!handle.valid() || device_ == VK_NULL_HANDLE) return;
    auto it = meshes_.find(handle.id);
    if (it == meshes_.end()) return;

    vkDeviceWaitIdle(device_);
    if (it->second.vb != VK_NULL_HANDLE) vkDestroyBuffer(device_, it->second.vb, nullptr);
    if (it->second.vb_mem != VK_NULL_HANDLE) vkFreeMemory(device_, it->second.vb_mem, nullptr);
    if (it->second.ib != VK_NULL_HANDLE) vkDestroyBuffer(device_, it->second.ib, nullptr);
    if (it->second.ib_mem != VK_NULL_HANDLE) vkFreeMemory(device_, it->second.ib_mem, nullptr);
    meshes_.erase(it);
}

void VulkanRenderer::draw_mesh(MeshHandle mesh, const Mat4& transform, TextureHandle texture) {
    if (!frame_active_ || graphics_pipeline_ == VK_NULL_HANDLE || !mesh.valid()) return;

    auto mit = meshes_.find(mesh.id);
    if (mit == meshes_.end()) return;
    const VkMesh& vk_mesh = mit->second;

    VkCommandBuffer cmd = command_buffers_[current_frame_];

    PushConstants pc{};
    pc.mvp  = mul_mat4(mul_mat4(transform, view_matrix_), proj_matrix_);
    pc.tint = Color::white();
    vkCmdPushConstants(cmd, pipeline_layout_,
                       VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
                       0, sizeof(PushConstants), &pc);

    TextureHandle tex_to_bind = texture.valid() ? texture : default_white_tex_;
    auto tit = textures_.find(tex_to_bind.id);
    if (tit != textures_.end() && tit->second.descriptor_set != VK_NULL_HANDLE) {
        vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline_layout_,
                                0, 1, &tit->second.descriptor_set, 0, nullptr);
    }

    VkDeviceSize offset = 0;
    vkCmdBindVertexBuffers(cmd, 0, 1, &vk_mesh.vb, &offset);
    if (vk_mesh.ib != VK_NULL_HANDLE && vk_mesh.index_count > 0) {
        vkCmdBindIndexBuffer(cmd, vk_mesh.ib, 0, VK_INDEX_TYPE_UINT32);
        vkCmdDrawIndexed(cmd, vk_mesh.index_count, 1, 0, 0, 0);
    }
}

void VulkanRenderer::draw_quad_2d(const RectF& rect, const Color& color, TextureHandle texture) {
    if (!frame_active_ || graphics_pipeline_ == VK_NULL_HANDLE || !unit_quad_mesh_.valid()) return;
    if (width_ == 0 || height_ == 0) return;

    auto mit = meshes_.find(unit_quad_mesh_.id);
    if (mit == meshes_.end()) return;
    const VkMesh& quad = mit->second;

    Mat4 model = Mat4::identity();
    model.m[0][0] = rect.w;
    model.m[1][1] = rect.h;
    model.m[3][0] = rect.x;
    model.m[3][1] = rect.y;

    PushConstants pc{};
    pc.mvp  = mul_mat4(model, ortho_2d(static_cast<f32>(width_), static_cast<f32>(height_)));
    pc.tint = color;

    VkCommandBuffer cmd = command_buffers_[current_frame_];
    vkCmdPushConstants(cmd, pipeline_layout_,
                       VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
                       0, sizeof(PushConstants), &pc);

    TextureHandle tex_to_bind = texture.valid() ? texture : default_white_tex_;
    auto tit = textures_.find(tex_to_bind.id);
    if (tit != textures_.end() && tit->second.descriptor_set != VK_NULL_HANDLE) {
        vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline_layout_,
                                0, 1, &tit->second.descriptor_set, 0, nullptr);
    }

    VkDeviceSize offset = 0;
    vkCmdBindVertexBuffers(cmd, 0, 1, &quad.vb, &offset);
    vkCmdBindIndexBuffer(cmd, quad.ib, 0, VK_INDEX_TYPE_UINT32);
    vkCmdDrawIndexed(cmd, quad.index_count, 1, 0, 0, 0);
}

void VulkanRenderer::draw_sprite(const Sprite& sprite, const Vec2& position,
                                 f32 /*rotation*/, f32 scale) {
    RectF rect{};
    rect.x = position.x - sprite.origin.x * scale;
    rect.y = position.y - sprite.origin.y * scale;
    rect.w = sprite.size.x * scale;
    rect.h = sprite.size.y * scale;
    draw_quad_2d(rect, sprite.tint, sprite.texture);
}

void VulkanRenderer::draw_rect(const RectF& rect, const Color& color, bool filled) {
    if (filled) {
        draw_quad_2d(rect, color, default_white_tex_);
        return;
    }
    const f32 t = 2.0f;
    draw_quad_2d({rect.x, rect.y, rect.w, t}, color, default_white_tex_);
    draw_quad_2d({rect.x, rect.y + rect.h - t, rect.w, t}, color, default_white_tex_);
    draw_quad_2d({rect.x, rect.y, t, rect.h}, color, default_white_tex_);
    draw_quad_2d({rect.x + rect.w - t, rect.y, t, rect.h}, color, default_white_tex_);
}

// ── Textures ─────────────────────────────────────────────────────────────────

TextureHandle VulkanRenderer::load_texture(const void* data, u32 width, u32 height,
                                           TextureFormat format) {
    if (device_ == VK_NULL_HANDLE || !data || width == 0 || height == 0) return {};

    VkFormat vk_fmt = get_vk_format(format);
    if (vk_fmt == VK_FORMAT_UNDEFINED) return {};

    const VkDeviceSize image_size = calculate_image_size(format, width, height);

    VkBuffer staging = VK_NULL_HANDLE;
    VkDeviceMemory staging_mem = VK_NULL_HANDLE;
    if (!create_buffer(image_size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                       VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                       staging, staging_mem)) {
        return {};
    }

    void* mapped = nullptr;
    vkMapMemory(device_, staging_mem, 0, image_size, 0, &mapped);
    std::memcpy(mapped, data, static_cast<size_t>(image_size));
    vkUnmapMemory(device_, staging_mem);

    VkTexture tex{};
    tex.width  = width;
    tex.height = height;

    VkImageCreateInfo ici{};
    ici.sType         = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    ici.imageType     = VK_IMAGE_TYPE_2D;
    ici.extent        = {width, height, 1};
    ici.mipLevels     = 1;
    ici.arrayLayers   = 1;
    ici.format        = vk_fmt;
    ici.tiling        = VK_IMAGE_TILING_OPTIMAL;
    ici.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    ici.usage         = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
    ici.samples       = VK_SAMPLE_COUNT_1_BIT;
    ici.sharingMode   = VK_SHARING_MODE_EXCLUSIVE;

    if (vkCreateImage(device_, &ici, nullptr, &tex.image) != VK_SUCCESS) {
        vkDestroyBuffer(device_, staging, nullptr);
        vkFreeMemory(device_, staging_mem, nullptr);
        return {};
    }

    VkMemoryRequirements mem_reqs{};
    vkGetImageMemoryRequirements(device_, tex.image, &mem_reqs);

    VkMemoryAllocateInfo mai{};
    mai.sType           = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    mai.allocationSize  = mem_reqs.size;
    mai.memoryTypeIndex = find_memory_type(mem_reqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

    if (vkAllocateMemory(device_, &mai, nullptr, &tex.memory) != VK_SUCCESS) {
        vkDestroyImage(device_, tex.image, nullptr);
        vkDestroyBuffer(device_, staging, nullptr);
        vkFreeMemory(device_, staging_mem, nullptr);
        return {};
    }
    vkBindImageMemory(device_, tex.image, tex.memory, 0);

    transition_image_layout(tex.image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
    copy_buffer_to_image(staging, tex.image, width, height);
    transition_image_layout(tex.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

    vkDestroyBuffer(device_, staging, nullptr);
    vkFreeMemory(device_, staging_mem, nullptr);

    VkImageViewCreateInfo ivci{};
    ivci.sType                           = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    ivci.image                           = tex.image;
    ivci.viewType                        = VK_IMAGE_VIEW_TYPE_2D;
    ivci.format                          = vk_fmt;
    ivci.subresourceRange.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
    ivci.subresourceRange.baseMipLevel   = 0;
    ivci.subresourceRange.levelCount     = 1;
    ivci.subresourceRange.baseArrayLayer = 0;
    ivci.subresourceRange.layerCount     = 1;

    if (vkCreateImageView(device_, &ivci, nullptr, &tex.view) != VK_SUCCESS) {
        vkDestroyImage(device_, tex.image, nullptr);
        vkFreeMemory(device_, tex.memory, nullptr);
        return {};
    }

    VkDescriptorSetAllocateInfo dsai{};
    dsai.sType              = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    dsai.descriptorPool     = descriptor_pool_;
    dsai.descriptorSetCount = 1;
    dsai.pSetLayouts        = &descriptor_layout_;

    if (vkAllocateDescriptorSets(device_, &dsai, &tex.descriptor_set) != VK_SUCCESS) {
        vkDestroyImageView(device_, tex.view, nullptr);
        vkDestroyImage(device_, tex.image, nullptr);
        vkFreeMemory(device_, tex.memory, nullptr);
        return {};
    }

    VkDescriptorImageInfo dii{};
    dii.sampler     = sampler_linear_;
    dii.imageView   = tex.view;
    dii.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

    VkWriteDescriptorSet write{};
    write.sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    write.dstSet          = tex.descriptor_set;
    write.dstBinding      = 0;
    write.descriptorCount = 1;
    write.descriptorType  = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    write.pImageInfo      = &dii;
    vkUpdateDescriptorSets(device_, 1, &write, 0, nullptr);

    TextureHandle h{};
    h.id = next_texture_id_++;
    textures_[h.id] = tex;
    return h;
}

TextureHandle VulkanRenderer::load_texture_from_file(const std::string& path) {
    auto parsed = formats::DdsLoader::load_from_file(path);
    if (!parsed.is_valid || parsed.pixel_data.empty()) {
        return {};
    }
    return load_texture(parsed.pixel_data.data(), parsed.width, parsed.height, parsed.format);
}

void VulkanRenderer::destroy_texture(TextureHandle handle) {
    if (!handle.valid() || device_ == VK_NULL_HANDLE) return;
    auto it = textures_.find(handle.id);
    if (it == textures_.end()) return;

    vkDeviceWaitIdle(device_);
    if (it->second.descriptor_set != VK_NULL_HANDLE) {
        vkFreeDescriptorSets(device_, descriptor_pool_, 1, &it->second.descriptor_set);
    }
    if (it->second.view != VK_NULL_HANDLE) vkDestroyImageView(device_, it->second.view, nullptr);
    if (it->second.image != VK_NULL_HANDLE) vkDestroyImage(device_, it->second.image, nullptr);
    if (it->second.memory != VK_NULL_HANDLE) vkFreeMemory(device_, it->second.memory, nullptr);
    textures_.erase(it);
}

// ── Camera & Settings ────────────────────────────────────────────────────────

void VulkanRenderer::set_view_matrix(const Mat4& view) {
    view_matrix_ = view;
}

void VulkanRenderer::set_projection_matrix(const Mat4& proj) {
    proj_matrix_ = proj;
}

void VulkanRenderer::get_resolution(u32& w, u32& h) const {
    w = width_;
    h = height_;
}

void VulkanRenderer::resize(u32 width, u32 height) {
    if (width == 0 || height == 0) return;
    width_               = width;
    height_              = height;
    framebuffer_resized_ = true;
}

void VulkanRenderer::set_vsync(bool enabled) {
    if (vsync_ == enabled) return;
    vsync_               = enabled;
    framebuffer_resized_ = true;
}

// ── Low-level Vulkan Helpers ─────────────────────────────────────────────────

u32 VulkanRenderer::find_memory_type(u32 type_filter, VkMemoryPropertyFlags properties) const {
    VkPhysicalDeviceMemoryProperties mem_props{};
    vkGetPhysicalDeviceMemoryProperties(physical_device_, &mem_props);

    for (u32 i = 0; i < mem_props.memoryTypeCount; ++i) {
        if ((type_filter & (1u << i)) &&
            (mem_props.memoryTypes[i].propertyFlags & properties) == properties) {
            return i;
        }
    }
    return 0;
}

VkFormat VulkanRenderer::find_depth_format() const {
    const std::array<VkFormat, 2> candidates = {
        VK_FORMAT_D32_SFLOAT,
        VK_FORMAT_D24_UNORM_S8_UINT,
    };
    for (VkFormat fmt : candidates) {
        VkFormatProperties props{};
        vkGetPhysicalDeviceFormatProperties(physical_device_, fmt, &props);
        if (props.optimalTilingFeatures & VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT) {
            return fmt;
        }
    }
    return VK_FORMAT_D32_SFLOAT;
}

bool VulkanRenderer::create_buffer(VkDeviceSize size, VkBufferUsageFlags usage,
                                   VkMemoryPropertyFlags properties,
                                   VkBuffer& buffer, VkDeviceMemory& memory) {
    VkBufferCreateInfo bci{};
    bci.sType       = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bci.size        = size;
    bci.usage       = usage;
    bci.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    if (vkCreateBuffer(device_, &bci, nullptr, &buffer) != VK_SUCCESS) return false;

    VkMemoryRequirements mem_reqs{};
    vkGetBufferMemoryRequirements(device_, buffer, &mem_reqs);

    VkMemoryAllocateInfo mai{};
    mai.sType           = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    mai.allocationSize  = mem_reqs.size;
    mai.memoryTypeIndex = find_memory_type(mem_reqs.memoryTypeBits, properties);

    if (vkAllocateMemory(device_, &mai, nullptr, &memory) != VK_SUCCESS) {
        vkDestroyBuffer(device_, buffer, nullptr);
        buffer = VK_NULL_HANDLE;
        return false;
    }

    vkBindBufferMemory(device_, buffer, memory, 0);
    return true;
}

VkCommandBuffer VulkanRenderer::begin_single_time_commands() {
    VkCommandBufferAllocateInfo cbai{};
    cbai.sType              = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    cbai.level              = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    cbai.commandPool        = command_pool_;
    cbai.commandBufferCount = 1;

    VkCommandBuffer cmd = VK_NULL_HANDLE;
    vkAllocateCommandBuffers(device_, &cbai, &cmd);

    VkCommandBufferBeginInfo bi{};
    bi.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    bi.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    vkBeginCommandBuffer(cmd, &bi);
    return cmd;
}

void VulkanRenderer::end_single_time_commands(VkCommandBuffer cmd) {
    vkEndCommandBuffer(cmd);

    VkSubmitInfo si{};
    si.sType              = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    si.commandBufferCount = 1;
    si.pCommandBuffers    = &cmd;

    vkQueueSubmit(graphics_queue_, 1, &si, VK_NULL_HANDLE);
    vkQueueWaitIdle(graphics_queue_);
    vkFreeCommandBuffers(device_, command_pool_, 1, &cmd);
}

void VulkanRenderer::copy_buffer(VkBuffer src, VkBuffer dst, VkDeviceSize size) {
    VkCommandBuffer cmd = begin_single_time_commands();
    VkBufferCopy region{0, 0, size};
    vkCmdCopyBuffer(cmd, src, dst, 1, &region);
    end_single_time_commands(cmd);
}

void VulkanRenderer::transition_image_layout(VkImage image, VkImageLayout old_layout,
                                             VkImageLayout new_layout) {
    VkCommandBuffer cmd = begin_single_time_commands();

    VkImageMemoryBarrier barrier{};
    barrier.sType                           = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.oldLayout                       = old_layout;
    barrier.newLayout                       = new_layout;
    barrier.srcQueueFamilyIndex             = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex             = VK_QUEUE_FAMILY_IGNORED;
    barrier.image                           = image;
    barrier.subresourceRange.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
    barrier.subresourceRange.baseMipLevel   = 0;
    barrier.subresourceRange.levelCount     = 1;
    barrier.subresourceRange.baseArrayLayer = 0;
    barrier.subresourceRange.layerCount     = 1;

    VkPipelineStageFlags src_stage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
    VkPipelineStageFlags dst_stage = VK_PIPELINE_STAGE_TRANSFER_BIT;

    if (old_layout == VK_IMAGE_LAYOUT_UNDEFINED &&
        new_layout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL) {
        barrier.srcAccessMask = 0;
        barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        src_stage             = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
        dst_stage             = VK_PIPELINE_STAGE_TRANSFER_BIT;
    } else if (old_layout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL &&
               new_layout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL) {
        barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
        src_stage             = VK_PIPELINE_STAGE_TRANSFER_BIT;
        dst_stage             = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
    }

    vkCmdPipelineBarrier(cmd, src_stage, dst_stage, 0, 0, nullptr, 0, nullptr, 1, &barrier);
    end_single_time_commands(cmd);
}

void VulkanRenderer::copy_buffer_to_image(VkBuffer buffer, VkImage image, u32 width, u32 height) {
    VkCommandBuffer cmd = begin_single_time_commands();

    VkBufferImageCopy region{};
    region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    region.imageSubresource.layerCount = 1;
    region.imageExtent                 = {width, height, 1};

    vkCmdCopyBufferToImage(cmd, buffer, image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);
    end_single_time_commands(cmd);
}

VkShaderModule VulkanRenderer::create_shader_module(const std::vector<u8>& code) {
    if (code.empty() || (code.size() % 4) != 0) return VK_NULL_HANDLE;

    VkShaderModuleCreateInfo smci{};
    smci.sType    = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    smci.codeSize = code.size();
    smci.pCode    = reinterpret_cast<const u32*>(code.data());

    VkShaderModule mod = VK_NULL_HANDLE;
    if (vkCreateShaderModule(device_, &smci, nullptr, &mod) != VK_SUCCESS) {
        return VK_NULL_HANDLE;
    }
    return mod;
}

} // namespace hotd3

#endif // HOTD3_HAS_VULKAN
