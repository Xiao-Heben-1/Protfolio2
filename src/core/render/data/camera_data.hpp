#ifndef __RENDER_CAMERA_DATA_HPP
#define __RENDER_CAMERA_DATA_HPP

#include <vulkan/vulkan.h>
#include <glm/glm.hpp>
#include <core/render/data/data.hpp>
namespace render{

/// @brief 摄像机 Uniform 数据
struct camera_uniform_data final{
    glm::mat4 view{1.0f};
    glm::mat4 projection{1.0f};
    glm::mat4 vp{1.0f};
};

/// @brief 摄像机数据资源
struct camera_data final:public gpu_data{
    camera_uniform_data uniform_data;

    VkDescriptorPool descriptor_pool = VK_NULL_HANDLE;

    //camera UBO
    VkBuffer uniform_buffer = VK_NULL_HANDLE;
    VkDeviceMemory uniform_memory = VK_NULL_HANDLE;
    void* uniform_mapped = nullptr;

    //颜色纹理
    VkImage color_image = VK_NULL_HANDLE;
    VkDeviceMemory color_memory = VK_NULL_HANDLE;
    VkImageView color_view = VK_NULL_HANDLE;
    VkSampler color_sampler = VK_NULL_HANDLE;

    // 透明合成后经曝光/ACES/sRGB编码的显示图像，独立于线性 HDR color_image。
    VkImage display_image=VK_NULL_HANDLE;
    VkDeviceMemory display_memory=VK_NULL_HANDLE;
    VkImageView display_view=VK_NULL_HANDLE;
    bool display_initialized=false;

    //法线纹理
    VkImage normal_image = VK_NULL_HANDLE;
    VkDeviceMemory normal_memory = VK_NULL_HANDLE;
    VkImageView normal_view = VK_NULL_HANDLE;
    VkSampler normal_sampler = VK_NULL_HANDLE;

    //深度纹理
    VkImage depth_image = VK_NULL_HANDLE;
    VkDeviceMemory depth_memory = VK_NULL_HANDLE;
    VkImageView depth_view = VK_NULL_HANDLE;
    VkSampler depth_sampler = VK_NULL_HANDLE;

    //camera UBO layout
    VkDescriptorSetLayout camera_layout = VK_NULL_HANDLE;

    //G-Buffer layout
    VkDescriptorSetLayout g_buffer_layout = VK_NULL_HANDLE;

    //descriptor sets
    VkDescriptorSet camera_set = VK_NULL_HANDLE;
    VkDescriptorSet g_buffer_set = VK_NULL_HANDLE;

    VkExtent2D extent{};
    VkFormat depth_format = VK_FORMAT_UNDEFINED;
    VkFormat g_buffer_format = VK_FORMAT_R16G16B16A16_SFLOAT;
    ~camera_data(){cleanup();}

    void create(VkPhysicalDevice physical_device) override;
    void update(VkPhysicalDevice physical_device) override;
    void upload(VkCommandBuffer command_buffer,VkPipelineLayout pipeline_layout) override;

    void init(VkDevice _device,VkExtent2D _extent,VkFormat _g_buffer_format,VkFormat _depth_format);
    void init(VkExtent2D _extent,VkFormat _g_buffer_format,VkFormat _depth_format);
    void cleanup() noexcept;
    void resize(VkPhysicalDevice physical_device,VkExtent2D size);
};

}

#endif // __RENDER_CAMERA_DATA_HPP
