#ifndef __RENDER_TEXTURE_HPP
#define __RENDER_TEXTURE_HPP

#include <vulkan/vulkan.h>
#include <string>

namespace render{
    // 材质持有的 GPU 纹理；图片注册和 CPU 像素由 image_data 管理。
    // 资源仍由所属 sub_material 释放，此结构不单独销毁 Vulkan 句柄。
    struct texture final{
        std::string image_uid;
        VkImage image = VK_NULL_HANDLE;
        VkDeviceMemory memory = VK_NULL_HANDLE;
        VkImageView view = VK_NULL_HANDLE;
        VkSampler sampler = VK_NULL_HANDLE;
        VkExtent2D extent{};
        VkFormat format = VK_FORMAT_UNDEFINED;
    };
    using material_texture = texture;
}

#endif // __RENDER_TEXTURE_HPP
