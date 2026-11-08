#ifndef __RENDER_LIGHT_DATA_HPP
#define __RENDER_LIGHT_DATA_HPP
#include <glm/glm.hpp>
#include <vulkan/vulkan.h>
#include <core/render/data/data.hpp>
#include <vector>
namespace render{
/// @brief 主光源 GPU 数据
struct main_light_uniform_data final{
    glm::vec4 direction{0,1,0,.004675f}; // xyz 指向太阳的世界单位方向；w 角半径 rad。
    glm::vec4 solar_irradiance{0,0,0,1}; // rgb 大气层外法向辐照度 W/m²；w 圆盘亮度倍率。
};

/// @brief 一般光源 GPU 数据
struct normal_light_uniform_data final{
    glm::vec4 position;//xyz位置
    glm::vec4 color;//rgb颜色
    glm::vec4 direction;//xyz方向
    glm::vec4 parameter;//x emission y k1 z k2
};

/// @brief 光源 GPU 数据资源
struct light_data final:public gpu_data{
    static constexpr uint32_t default_max_count = 1024;

    main_light_uniform_data main_light_data;
    std::vector<normal_light_uniform_data> light_datas;
    std::vector<uint32_t> light_indices;

    //主光源 UBO
    VkBuffer main_light_buffer = VK_NULL_HANDLE;
    VkDeviceMemory main_light_memory = VK_NULL_HANDLE;
    void* main_light_mapped = nullptr;

    //普通光源 SSBO
    VkBuffer light_buffer = VK_NULL_HANDLE;
    VkDeviceMemory light_memory = VK_NULL_HANDLE;
    void* light_mapped = nullptr;

    //光源计数 UBO
    VkBuffer light_count_buffer = VK_NULL_HANDLE;
    VkDeviceMemory light_count_memory = VK_NULL_HANDLE;
    void* light_count_mapped = nullptr;

    uint32_t max_count = default_max_count;//当前 SSBO 容量
    uint32_t light_count = 0;

    //光源 descriptor
    VkDescriptorPool descriptor_pool = VK_NULL_HANDLE;//光源 descriptor pool
    VkDescriptorSetLayout light_layout = VK_NULL_HANDLE;//set 1 layout
    VkDescriptorSet light_set = VK_NULL_HANDLE;//set 1 descriptor set

    explicit light_data(){light_datas.reserve(1024);light_indices.reserve(1024);}
    ~light_data(){cleanup();};
    void create(VkPhysicalDevice physical_device) override;
    void update(VkPhysicalDevice physical_device) override;
    void upload(VkCommandBuffer command_buffer,VkPipelineLayout pipeline_layout) override;
    void cleanup() noexcept override;

private:
    uint32_t resize_count = 0;

    /// @brief 扩容系数 
    float resize_factor() const noexcept;

    void update_main_light() noexcept;
    void update_lights(VkPhysicalDevice physical_device);
    void create_buffer(VkPhysicalDevice physical_device);
    void resize_buffer(VkPhysicalDevice physical_device);
};

}

#endif
