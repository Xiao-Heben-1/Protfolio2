#ifndef __RENDER_OIT_DATA_HPP
#define __RENDER_OIT_DATA_HPP

#include <core/render/data/data.hpp>
#include <cstdint>

namespace render{
    // 每像素固定 16 项最大堆与 16 个颜色槽，只随分辨率重建存储。
    class oit_data final:public gpu_data{
        friend class render_system;
        friend class render_resources;

    private:
        static constexpr uint32_t heap_size = 16;
        oit_data() = default;
        ~oit_data() override{cleanup();}
        oit_data(const oit_data&) = delete;
        oit_data& operator=(const oit_data&) = delete;

        VkDescriptorSetLayout layout = VK_NULL_HANDLE;
        VkDescriptorSet descriptor_set = VK_NULL_HANDLE;
        using gpu_data::init;
        void init(VkDevice device, VkExtent2D extent);
        void create(VkPhysicalDevice physical_device) override;
        // 分辨率改变时重建图像和 SSBO；不按片元数量扩容。
        void update(VkPhysicalDevice physical_device) override;
        // 更新堆图像状态并绑定 descriptor set。
        void upload(VkCommandBuffer command_buffer,VkPipelineLayout pipeline_layout) override;
        void cleanup() noexcept override;

        struct buffer{
            VkBuffer handle = VK_NULL_HANDLE;
            VkDeviceMemory memory = VK_NULL_HANDLE;
            VkDeviceSize size = 0;
        } nodes;
        static constexpr VkDeviceSize node_stride = 16;//oit_fragment 仅含 vec4 color
        VkImage heap_image = VK_NULL_HANDLE;
        VkDeviceMemory heap_memory = VK_NULL_HANDLE;
        VkImageView heap_view = VK_NULL_HANDLE;
        bool heap_initialized = false;
        VkPhysicalDevice physical_device = VK_NULL_HANDLE;
        VkExtent2D extent{};
        VkExtent2D storage_extent{};
        VkDescriptorPool pool = VK_NULL_HANDLE;
        buffer create_buffer(VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties);
        void destroy_buffer(buffer& value) noexcept;
        void create_storage();
        void destroy_storage() noexcept;
        void update_descriptors();
    };
}

#endif // __RENDER_OIT_DATA_HPP
