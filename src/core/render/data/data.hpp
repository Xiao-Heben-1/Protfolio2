#ifndef __RENDER_DATA_HPP
#define __RENDER_DATA_HPP
#include <vulkan/vulkan.h>
#include <stdexcept>
namespace render{

    class gpu_data{
    public:
        virtual ~gpu_data() = default;
        virtual void init(VkDevice _device){device = _device;}
        virtual void create(VkPhysicalDevice physical_device)= 0;//创建缓存
        virtual void update(VkPhysicalDevice physical_device) = 0;//更新数据
        virtual void upload(VkCommandBuffer command_buffer,VkPipelineLayout pipeline_layout) = 0;//更新数据
        virtual void cleanup() noexcept = 0;
    protected:
        VkDevice device = VK_NULL_HANDLE;
        uint32_t find_memory_type(VkPhysicalDevice physical_device,uint32_t type_filter,VkMemoryPropertyFlags properties);//查找内存
    };

    inline uint32_t gpu_data::find_memory_type(VkPhysicalDevice physical_device,uint32_t type_filter,VkMemoryPropertyFlags properties){
        VkPhysicalDeviceMemoryProperties memory_properties{};
        vkGetPhysicalDeviceMemoryProperties(physical_device,&memory_properties);

        for(uint32_t i = 0;i < memory_properties.memoryTypeCount;i++){
            if((type_filter & (1u << i)) && (memory_properties.memoryTypes[i].propertyFlags & properties) == properties){
                return i;
            }
        }
        throw std::runtime_error("No compatible Vulkan memory type");
    }
}

#endif // __RENDER_DATA_HPP
