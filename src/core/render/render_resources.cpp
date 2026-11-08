#include <core/render/render_resources.hpp>
#include <engine.hpp>
#include <stdexcept>
#include <core/component/component_enter.hpp>
namespace render{

    render_resources::render_resources(VkPhysicalDevice physical_device,VkDevice device):
        physical_device(physical_device),device(device),command_pool(VK_NULL_HANDLE),graphics_queue(VK_NULL_HANDLE){
        init();
    }

   render_resources::~render_resources(){
        cleanup();
    }
    void render_resources::init(VkPhysicalDevice _physical_device,VkDevice _device,uint32_t graphics_queue_family){
        physical_device = _physical_device;
        device = _device;

        //获取 graphics queue
        vkGetDeviceQueue(device,graphics_queue_family,0,&graphics_queue);
        //创建 command pool
        VkCommandPoolCreateInfo command_pool_info{};
        command_pool_info.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
        command_pool_info.queueFamilyIndex = graphics_queue_family;
        command_pool_info.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
        if(vkCreateCommandPool(device,&command_pool_info,nullptr,&command_pool) != VK_SUCCESS){
            throw std::runtime_error("Failed to create command pool");
        }
        //初始化 GPU data
        cam.init(device,egn.get_render_system().get_scene().get_main_camera().extent,egn.get_render_system().get_scene().get_main_camera().g_buffer_format,egn.get_render_system().get_scene().get_main_camera().depth_format);
        light.init(device);
        cam.create(physical_device);
        light.create(physical_device);
        sky.init(device);sky.create(physical_device);
    }
    void render_resources::init(){
        cam.init(device,egn.get_render_system().get_scene().get_main_camera().extent,egn.get_render_system().get_scene().get_main_camera().g_buffer_format,egn.get_render_system().get_scene().get_main_camera().depth_format);
        light.init(device);
        cam.create(physical_device);
        light.create(physical_device);
        sky.init(device);sky.create(physical_device);
    }
    void render_resources::update() noexcept{
        cam.update(physical_device);
        light.update(physical_device);
        sky.update(physical_device);
    }
    void render_resources::upload(VkCommandBuffer command_buffer,VkPipelineLayout pipeline_layout) noexcept{
        cam.upload(command_buffer,pipeline_layout);
        light.upload(command_buffer,pipeline_layout);
        sky.upload(command_buffer,pipeline_layout);
    }
    void render_resources::cleanup() noexcept{
        if(device == VK_NULL_HANDLE) return;
        cam.cleanup();
        light.cleanup();
        sky.cleanup();
        oit.cleanup();
        vkDestroyCommandPool(device,command_pool,nullptr);
        command_pool = VK_NULL_HANDLE;
        device = VK_NULL_HANDLE;
    }
}
