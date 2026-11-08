#include <core/render/data/oit_data.hpp>

namespace render{
oit_data::buffer oit_data::create_buffer(VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags flags){
    buffer result{};
    result.size = size;
    VkBufferCreateInfo info{VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
    info.size = size;
    info.usage = usage;
    info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    vkCreateBuffer(device,&info,nullptr,&result.handle);
    VkMemoryRequirements requirements{};
    vkGetBufferMemoryRequirements(device,result.handle,&requirements);
    VkMemoryAllocateInfo allocation{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
    allocation.allocationSize = requirements.size;
    allocation.memoryTypeIndex = find_memory_type(physical_device,requirements.memoryTypeBits,flags);
    vkAllocateMemory(device,&allocation,nullptr,&result.memory);
    vkBindBufferMemory(device,result.handle,result.memory,0);
    return result;
}

void oit_data::destroy_buffer(buffer& value) noexcept{
    vkDestroyBuffer(device,value.handle,nullptr);
    vkFreeMemory(device,value.memory,nullptr);
    value = {};
}

void oit_data::init(VkDevice logical, VkExtent2D size){
    gpu_data::init(logical);
    extent = size;
}


void oit_data::create_storage(){
    const VkDeviceSize slots = VkDeviceSize(extent.width) * extent.height * heap_size;
    nodes = create_buffer(slots * node_stride,VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

    VkImageCreateInfo info{VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};
    info.imageType = VK_IMAGE_TYPE_2D;
    info.format = VK_FORMAT_R64_UINT;
    info.extent = {extent.width,extent.height,1};
    info.mipLevels = 1;
    info.arrayLayers = heap_size;
    info.samples = VK_SAMPLE_COUNT_1_BIT;
    info.tiling = VK_IMAGE_TILING_OPTIMAL;
    info.usage = VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    vkCreateImage(device,&info,nullptr,&heap_image);
    VkMemoryRequirements requirements{};
    vkGetImageMemoryRequirements(device,heap_image,&requirements);
    VkMemoryAllocateInfo allocation{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
    allocation.allocationSize = requirements.size;
    allocation.memoryTypeIndex = find_memory_type(physical_device,requirements.memoryTypeBits,VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    vkAllocateMemory(device,&allocation,nullptr,&heap_memory);
    vkBindImageMemory(device,heap_image,heap_memory,0);

    VkImageViewCreateInfo view{VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
    view.image = heap_image;
    view.viewType = VK_IMAGE_VIEW_TYPE_2D_ARRAY;
    view.format = VK_FORMAT_R64_UINT;
    view.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT,0,1,0,heap_size};
    vkCreateImageView(device,&view,nullptr,&heap_view);
    storage_extent = extent;
    heap_initialized = false;
}

void oit_data::destroy_storage() noexcept{
    destroy_buffer(nodes);
    vkDestroyImageView(device,heap_view,nullptr);
    vkDestroyImage(device,heap_image,nullptr);
    vkFreeMemory(device,heap_memory,nullptr);
    heap_view = VK_NULL_HANDLE;
    heap_image = VK_NULL_HANDLE;
    heap_memory = VK_NULL_HANDLE;
    storage_extent = {};
    heap_initialized = false;
}

void oit_data::create(VkPhysicalDevice physical){
    physical_device = physical;
    VkDescriptorSetLayoutBinding bindings[2]{};
    const VkShaderStageFlags stages = VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_COMPUTE_BIT;
    bindings[0] = {0,VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,1,stages,nullptr};
    bindings[1] = {2,VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,1,stages,nullptr};
    VkDescriptorSetLayoutCreateInfo layout_info{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO};
    layout_info.bindingCount = 2;
    layout_info.pBindings = bindings;
    vkCreateDescriptorSetLayout(device,&layout_info,nullptr,&layout);
    VkDescriptorPoolSize sizes[]{{VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,1},{VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,1}};
    VkDescriptorPoolCreateInfo pool_info{VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO};
    pool_info.maxSets = 1;
    pool_info.poolSizeCount = 2;
    pool_info.pPoolSizes = sizes;
    vkCreateDescriptorPool(device,&pool_info,nullptr,&pool);
    VkDescriptorSetAllocateInfo allocate{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO};
    allocate.descriptorPool = pool;
    allocate.descriptorSetCount = 1;
    allocate.pSetLayouts = &layout;
    vkAllocateDescriptorSets(device,&allocate,&descriptor_set);
    create_storage();
    update_descriptors();
}

void oit_data::update(VkPhysicalDevice physical){
    physical_device = physical;
    if(extent.width == storage_extent.width && extent.height == storage_extent.height) return;
    // 上一帧已提交完成；只重建分辨率相关资源，保留 layout 和 descriptor set。
    destroy_storage();
    create_storage();
    update_descriptors();
}

void oit_data::update_descriptors(){
    VkDescriptorBufferInfo colors{nodes.handle,0,nodes.size};
    VkDescriptorImageInfo heap{VK_NULL_HANDLE,heap_view,VK_IMAGE_LAYOUT_GENERAL};
    VkWriteDescriptorSet writes[2]{};
    writes[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writes[0].dstSet = descriptor_set;
    writes[0].dstBinding = 0;
    writes[0].descriptorCount = 1;
    writes[0].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    writes[0].pBufferInfo = &colors;
    writes[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writes[1].dstSet = descriptor_set;
    writes[1].dstBinding = 2;
    writes[1].descriptorCount = 1;
    writes[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
    writes[1].pImageInfo = &heap;
    vkUpdateDescriptorSets(device,2,writes,0,nullptr);
}

void oit_data::upload(VkCommandBuffer command_buffer,VkPipelineLayout pipeline_layout){
    VkImageMemoryBarrier before{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
    before.srcAccessMask = heap_initialized ? VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT : 0;
    before.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    before.oldLayout = heap_initialized ? VK_IMAGE_LAYOUT_GENERAL : VK_IMAGE_LAYOUT_UNDEFINED;
    before.newLayout = VK_IMAGE_LAYOUT_GENERAL;
    before.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    before.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    before.image = heap_image;
    before.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT,0,1,0,heap_size};
    const VkPipelineStageFlags shader_stages = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT | VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
    vkCmdPipelineBarrier(command_buffer,heap_initialized ? shader_stages : VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
        VK_PIPELINE_STAGE_TRANSFER_BIT,0,0,nullptr,0,nullptr,1,&before);
    // 零表示空槽，不需要额外计数层，也无需清空未被引用的旧颜色。
    VkClearColorValue clear{};
    vkCmdClearColorImage(command_buffer,heap_image,VK_IMAGE_LAYOUT_GENERAL,&clear,1,&before.subresourceRange);
    heap_initialized = true;
    VkMemoryBarrier after{VK_STRUCTURE_TYPE_MEMORY_BARRIER};
    after.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT | VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT;
    after.dstAccessMask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT;
    vkCmdPipelineBarrier(command_buffer,VK_PIPELINE_STAGE_TRANSFER_BIT | shader_stages,
        shader_stages,0,1,&after,0,nullptr,0,nullptr);
    vkCmdBindDescriptorSets(command_buffer,VK_PIPELINE_BIND_POINT_GRAPHICS,pipeline_layout,4,1,&descriptor_set,0,nullptr);
}

void oit_data::cleanup() noexcept{
    if(device == VK_NULL_HANDLE) return;
    destroy_storage();
    vkDestroyDescriptorPool(device,pool,nullptr);
    vkDestroyDescriptorSetLayout(device,layout,nullptr);
    pool = VK_NULL_HANDLE;
    layout = VK_NULL_HANDLE;
    descriptor_set = VK_NULL_HANDLE;
    device = VK_NULL_HANDLE;
}
}
