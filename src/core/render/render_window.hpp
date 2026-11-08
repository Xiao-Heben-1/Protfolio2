#pragma once
#include <vulkan/vulkan.h>
#include <functional>
#include <vector>
namespace render {
struct render_window_info {
    std::vector<const char*> instance_extensions;
    std::function<VkSurfaceKHR(VkInstance)> create_surface;
};
struct render_context {
    VkInstance instance=VK_NULL_HANDLE;
    VkPhysicalDevice physical_device=VK_NULL_HANDLE;
    VkDevice device=VK_NULL_HANDLE;
    VkQueue queue=VK_NULL_HANDLE;
    uint32_t queue_family=0;
    VkSurfaceKHR surface=VK_NULL_HANDLE; // 窗口端在 renderer 销毁前释放
};
struct render_image {
    VkImageView view=VK_NULL_HANDLE;
    VkSampler sampler=VK_NULL_HANDLE;
    VkExtent2D extent{};
};
}
