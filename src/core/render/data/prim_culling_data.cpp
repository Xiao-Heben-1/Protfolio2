#include <core/render/data/prim_culling_data.hpp>
#include <algorithm>
#include <cstring>

namespace render {
namespace {
struct alignas(16) prim_cull_params {
    std::array<std::array<float,4>,6> planes{}; // 朝内，dot(n,p)+d >= 0
    uint32_t candidate_count = 0, visible_capacity = 0, padding0 = 0, padding1 = 0;
};
// Vulkan 0 <= z <= w：近面取 row2，远面取 row3-row2（也兼容 reverse-Z）。
prim_cull_params make_prim_cull_params(const glm::mat4& vp,uint32_t count,uint32_t capacity) {
    glm::vec4 r[4];
    for(int row = 0; row < 4; ++row) for(int col = 0; col < 4; ++col) {
        r[row][col] = vp[col][row];
    }
    const glm::vec4 p[6] = {r[3]+r[0],r[3]-r[0],r[3]+r[1],r[3]-r[1],r[2],r[3]-r[2]};
    prim_cull_params out; out.candidate_count = count; out.visible_capacity = capacity;
    for(int i = 0; i < 6; ++i) for(int j = 0; j < 4; ++j) out.planes[i][j] = p[i][j];
    return out;
}
}

prim_culling_data::~prim_culling_data() { cleanup(); }
VkBuffer prim_culling_data::indirect_buffer() const noexcept { return storage_[4].handle; }

prim_culling_data::buffer prim_culling_data::create_buffer(VkDeviceSize size,VkBufferUsageFlags usage,bool host) {
    buffer out{}; out.size = size;
    VkBufferCreateInfo info{VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
    info.size = size; info.usage = usage; info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    vkCreateBuffer(device,&info,nullptr,&out.handle);
    VkMemoryRequirements required{}; vkGetBufferMemoryRequirements(device,out.handle,&required);
    VkPhysicalDeviceMemoryProperties memory{}; vkGetPhysicalDeviceMemoryProperties(physical_,&memory);
    const VkMemoryPropertyFlags flags = host ? VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT : VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
    uint32_t type = UINT32_MAX;
    for(uint32_t i=0;i<memory.memoryTypeCount;++i)
        if((required.memoryTypeBits & (1u<<i)) && (memory.memoryTypes[i].propertyFlags & flags) == flags) { type=i; break; }
    VkMemoryAllocateInfo allocation{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
    allocation.allocationSize = required.size; allocation.memoryTypeIndex = type;
    vkAllocateMemory(device,&allocation,nullptr,&out.memory);
    vkBindBufferMemory(device,out.handle,out.memory,0);
    if(host) vkMapMemory(device,out.memory,0,size,0,&out.mapped);
    return out;
}
void prim_culling_data::destroy(buffer& b) noexcept {
    if(b.mapped) vkUnmapMemory(device,b.memory);
    vkDestroyBuffer(device,b.handle,nullptr); vkFreeMemory(device,b.memory,nullptr); b = {};
}
void prim_culling_data::init(VkPhysicalDevice physical,VkDevice logical) {
    gpu_data::init(logical);create(physical);
}
void prim_culling_data::create(VkPhysicalDevice physical){
    physical_ = physical;
    VkPhysicalDeviceProperties props{}; vkGetPhysicalDeviceProperties(physical,&props); limits_ = props.limits;
    VkDescriptorSetLayoutBinding bindings[5]{};
    for(uint32_t i=0;i<5;++i) bindings[i] = {i,i==1 ? VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER : VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,1,VK_SHADER_STAGE_COMPUTE_BIT,nullptr};
    VkDescriptorSetLayoutCreateInfo info{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO};
    info.bindingCount = 5; info.pBindings = bindings;
    vkCreateDescriptorSetLayout(device,&info,nullptr,&layout);
    const VkDescriptorPoolSize sizes[]{{VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,4},{VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,1}};
    VkDescriptorPoolCreateInfo pool{VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO};
    pool.maxSets = 1; pool.poolSizeCount = 2; pool.pPoolSizes = sizes;
    vkCreateDescriptorPool(device,&pool,nullptr,&pool_);
    VkDescriptorSetAllocateInfo allocate{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO};
    allocate.descriptorPool = pool_; allocate.descriptorSetCount = 1; allocate.pSetLayouts = &layout;
    vkAllocateDescriptorSets(device,&allocate,&set_);
    reserve(1);
}
void prim_culling_data::reserve(uint32_t count) {
    if(count <= capacity_) return;
    const uint32_t capacity = std::max(count,capacity_ * 2);
    buffer next[5]{};
    next[0] = create_buffer(VkDeviceSize(capacity)*sizeof(prim_cull_item),VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,true);
    next[1] = create_buffer(sizeof(prim_cull_params),VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,true);
    next[2] = create_buffer(VkDeviceSize(capacity)*16,VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,false);
    next[3] = create_buffer(8,VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,false);
    next[4] = create_buffer(VkDeviceSize(capacity)*sizeof(VkDrawIndexedIndirectCommand),VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT,true);
    VkDescriptorBufferInfo infos[5]{}; VkWriteDescriptorSet writes[5]{};
    for(uint32_t i=0;i<5;++i) {
        destroy(storage_[i]); storage_[i] = next[i];
        infos[i] = {storage_[i].handle,0,storage_[i].size};
        writes[i].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        writes[i].dstSet = set_; writes[i].dstBinding = i; writes[i].descriptorCount = 1;
        writes[i].descriptorType = i==1 ? VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER : VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        writes[i].pBufferInfo = &infos[i];
    }
    vkUpdateDescriptorSets(device,5,writes,0,nullptr); capacity_ = capacity;
}
void prim_culling_data::prepare(const std::vector<prim_cull_item>& candidates,
    const std::vector<VkDrawIndexedIndirectCommand>& commands,const glm::mat4& vp) {
    count_ = uint32_t(candidates.size()); update(physical_);
    const auto params = make_prim_cull_params(vp,count_,count_);
    std::memcpy(storage_[1].mapped,&params,sizeof(params));
    if(count_) {
        std::memcpy(storage_[0].mapped,candidates.data(),candidates.size()*sizeof(prim_cull_item));
        std::memcpy(storage_[4].mapped,commands.data(),commands.size()*sizeof(VkDrawIndexedIndirectCommand));
    }
}
void prim_culling_data::update(VkPhysicalDevice physical){physical_=physical;reserve(std::max(1u,count_));}
void prim_culling_data::upload(VkCommandBuffer command,VkPipelineLayout pipeline_layout){
    vkCmdBindDescriptorSets(command,VK_PIPELINE_BIND_POINT_COMPUTE,pipeline_layout,0,1,&set_,0,nullptr);
}
void prim_culling_data::dispatch(VkCommandBuffer cmd,VkPipeline pipeline,VkPipelineLayout pipeline_layout) {
    vkCmdFillBuffer(cmd,storage_[3].handle,0,8,0);
    VkMemoryBarrier before{VK_STRUCTURE_TYPE_MEMORY_BARRIER};
    before.srcAccessMask = VK_ACCESS_HOST_WRITE_BIT | VK_ACCESS_TRANSFER_WRITE_BIT;
    before.dstAccessMask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT | VK_ACCESS_INDIRECT_COMMAND_READ_BIT;
    vkCmdPipelineBarrier(cmd,VK_PIPELINE_STAGE_HOST_BIT | VK_PIPELINE_STAGE_TRANSFER_BIT,VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT | VK_PIPELINE_STAGE_DRAW_INDIRECT_BIT,0,1,&before,0,nullptr,0,nullptr);
    if(!count_) return;
    vkCmdBindPipeline(cmd,VK_PIPELINE_BIND_POINT_COMPUTE,pipeline);
    upload(cmd,pipeline_layout);
    const uint64_t batch = uint64_t(limits_.maxComputeWorkGroupCount[0])*64;
    for(uint64_t base=0;base<count_;base+=batch) {
        if(base){
            VkMemoryBarrier between{VK_STRUCTURE_TYPE_MEMORY_BARRIER};
            between.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
            between.dstAccessMask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT;
            vkCmdPipelineBarrier(cmd,VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,0,1,&between,0,nullptr,0,nullptr);
        }
        const uint32_t offset = uint32_t(base);
        vkCmdPushConstants(cmd,pipeline_layout,VK_SHADER_STAGE_COMPUTE_BIT,0,4,&offset);
        const auto n = std::min(uint64_t(count_)-base,batch);
        vkCmdDispatch(cmd,uint32_t((n+63)/64),1,1);
    }
    VkMemoryBarrier after{VK_STRUCTURE_TYPE_MEMORY_BARRIER};
    after.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
    after.dstAccessMask = VK_ACCESS_INDIRECT_COMMAND_READ_BIT;
    vkCmdPipelineBarrier(cmd,VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,VK_PIPELINE_STAGE_DRAW_INDIRECT_BIT,0,1,&after,0,nullptr,0,nullptr);
}
void prim_culling_data::cleanup() noexcept {
    if(!device) return;
    vkDestroyDescriptorPool(device,pool_,nullptr);
    for(auto& b : storage_) destroy(b);
    vkDestroyDescriptorSetLayout(device,layout,nullptr);
    pool_ = VK_NULL_HANDLE; layout = VK_NULL_HANDLE; set_ = VK_NULL_HANDLE;
    device = VK_NULL_HANDLE; capacity_ = count_ = 0;
}
}


