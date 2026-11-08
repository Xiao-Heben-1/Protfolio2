#pragma once
#include <array>
#include <cstdint>
#include <vector>
#include <glm/glm.hpp>
#include <vulkan/vulkan.h>
#include <core/render/data/data.hpp>
namespace render {
// std430 ABI：与 pre_processing/frustum_cull.comp 一致。
struct alignas(16) prim_cull_item {
    std::array<float,4> bounds_min{}, bounds_max{};
    // 当前帧候选实例数组下标。
    uint32_t instance_index = 0, lod = 0, transparent = 0, padding = 0;
};

// 当前 renderer 每帧 queueWaitIdle，prepare/扩容只在上一帧结束后调用。
// 将来多帧并行时，每个在途帧必须拥有独立实例。
class prim_culling_data:public gpu_data {
private:
    struct buffer { VkBuffer handle = VK_NULL_HANDLE; VkDeviceMemory memory = VK_NULL_HANDLE; void* mapped = nullptr; VkDeviceSize size = 0; };
    VkPhysicalDevice physical_ = VK_NULL_HANDLE;
    VkDescriptorPool pool_ = VK_NULL_HANDLE;
    VkDescriptorSet set_ = VK_NULL_HANDLE;
    VkPhysicalDeviceLimits limits_{};
    buffer storage_[5]{};
    uint32_t capacity_ = 0, count_ = 0;
public:
    VkDescriptorSetLayout layout = VK_NULL_HANDLE;

    prim_culling_data() = default;
    ~prim_culling_data();
    prim_culling_data(const prim_culling_data&) = delete;
    prim_culling_data& operator=(const prim_culling_data&) = delete;
    using gpu_data::init;
    void init(VkPhysicalDevice physical,VkDevice logical);
    void create(VkPhysicalDevice physical) override;
    void update(VkPhysicalDevice physical) override;
    void upload(VkCommandBuffer command,VkPipelineLayout pipeline_layout) override;
    void prepare(const std::vector<prim_cull_item>& candidates,
                 const std::vector<VkDrawIndexedIndirectCommand>& commands,const glm::mat4& vp);
    void dispatch(VkCommandBuffer command,VkPipeline pipeline,VkPipelineLayout pipeline_layout);
    void cleanup() noexcept override;
    VkBuffer indirect_buffer() const noexcept;
private:
    buffer create_buffer(VkDeviceSize size,VkBufferUsageFlags usage,bool host);
    void destroy(buffer& value) noexcept;
    void reserve(uint32_t count);
};
}

