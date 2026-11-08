#ifndef __RENDER_RESOURCES_HPP
#define __RENDER_RESOURCES_HPP

#include <vulkan/vulkan.h>
#include <core/render/data/data_enter.hpp>
#include <core/render/data/oit_data.hpp>

namespace render{
class render_system;
class object;
class normal_light;
class main_light;
class render_resources final{
    friend class render_system;
    friend class object;
    friend class normal_light;
    friend class main_light;
public:
    explicit render_resources(VkPhysicalDevice physical_device, VkDevice device);
    ~render_resources();
private:
    // render_system creates the device before initializing GPU resources.
    render_resources() = default;
    // 借用设备；resources 只销毁自己创建的 GPU 资源。
    VkPhysicalDevice physical_device = VK_NULL_HANDLE;
    VkDevice device = VK_NULL_HANDLE;
    VkCommandPool command_pool = VK_NULL_HANDLE;
    VkQueue graphics_queue = VK_NULL_HANDLE;

    camera_data cam;///摄像机数据
    light_data light;///光源数据
    skybox_data sky;///大气与地面，太阳参数来自 light.main_light_data。
    oit_data oit;///每像素 16 层透明片元堆


    void init();
    void init(VkPhysicalDevice _physical_device, VkDevice _device,uint32_t graphics_queue_family);
    void update() noexcept;
    void upload(VkCommandBuffer command_buffer, VkPipelineLayout pipeline_layout) noexcept;
    void cleanup() noexcept;
};
}
#endif // __RENDER_RESOURCES_HPP
