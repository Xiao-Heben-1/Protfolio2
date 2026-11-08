#include <core/render/data/light_data.hpp>
#include <cstring>
#include <cmath>
#include <stdexcept>

namespace render{

float light_data::resize_factor() const noexcept{
    return 1.0f + 0.5f / static_cast<float>(resize_count + 1);
}

void light_data::create_buffer(VkPhysicalDevice physical_device){
    VkDeviceSize buffer_size = sizeof(normal_light_uniform_data) * max_count;

    VkBufferCreateInfo buffer_info{};
    buffer_info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    buffer_info.size = buffer_size;
    buffer_info.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
    buffer_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    vkCreateBuffer(device,&buffer_info,nullptr,&light_buffer);

    VkMemoryRequirements memory_requirements{};
    vkGetBufferMemoryRequirements(device,light_buffer,&memory_requirements);

    VkMemoryAllocateInfo allocate_info{};
    allocate_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocate_info.allocationSize = memory_requirements.size;
    allocate_info.memoryTypeIndex = find_memory_type(physical_device,memory_requirements.memoryTypeBits,VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

    vkAllocateMemory(device,&allocate_info,nullptr,&light_memory);
    vkBindBufferMemory(device,light_buffer,light_memory,0);
    vkMapMemory(device,light_memory,0,buffer_size,0,&light_mapped);
}

void light_data::resize_buffer(VkPhysicalDevice physical_device){
    uint32_t new_count = static_cast<uint32_t>(std::ceil(static_cast<float>(max_count) * resize_factor()));
    if(new_count < light_count) new_count = light_count;
    //释放旧 SSBO
    vkUnmapMemory(device,light_memory);
    vkDestroyBuffer(device,light_buffer,nullptr);
    vkFreeMemory(device,light_memory,nullptr);

    light_buffer = VK_NULL_HANDLE;
    light_memory = VK_NULL_HANDLE;
    light_mapped = nullptr;

    //先更新容量，再按新容量创建 SSBO
    max_count = new_count;
    create_buffer(physical_device);
    VkDescriptorBufferInfo buffer{light_buffer,0,sizeof(normal_light_uniform_data)*max_count};
    VkWriteDescriptorSet write{VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
    write.dstSet=light_set;write.dstBinding=1;write.descriptorCount=1;
    write.descriptorType=VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;write.pBufferInfo=&buffer;
    vkUpdateDescriptorSets(device,1,&write,0,nullptr);
    resize_count++;
}

void light_data::create(VkPhysicalDevice physical_device){
    max_count = default_max_count;
    resize_count = 0;

    //创建主光源 UBO
    VkBufferCreateInfo buffer_info{};
    buffer_info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    buffer_info.size = sizeof(main_light_uniform_data);
    buffer_info.usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
    buffer_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    vkCreateBuffer(device,&buffer_info,nullptr,&main_light_buffer);

    VkMemoryRequirements memory_requirements{};
    vkGetBufferMemoryRequirements(device,main_light_buffer,&memory_requirements);

    VkMemoryAllocateInfo allocate_info{};
    allocate_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocate_info.allocationSize = memory_requirements.size;
    allocate_info.memoryTypeIndex = find_memory_type(physical_device,memory_requirements.memoryTypeBits,VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

    vkAllocateMemory(device,&allocate_info,nullptr,&main_light_memory);
    vkBindBufferMemory(device,main_light_buffer,main_light_memory,0);
    vkMapMemory(device,main_light_memory,0,sizeof(main_light_uniform_data),0,&main_light_mapped);

    //创建普通光源数量 UBO
    buffer_info.size = sizeof(uint32_t);

    vkCreateBuffer(device,&buffer_info,nullptr,&light_count_buffer);

    vkGetBufferMemoryRequirements(device,light_count_buffer,&memory_requirements);

    allocate_info.allocationSize = memory_requirements.size;
    allocate_info.memoryTypeIndex = find_memory_type(physical_device,memory_requirements.memoryTypeBits,VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

    vkAllocateMemory(device,&allocate_info,nullptr,&light_count_memory);
    vkBindBufferMemory(device,light_count_buffer,light_count_memory,0);
    vkMapMemory(device,light_count_memory,0,sizeof(uint32_t),0,&light_count_mapped);

    //创建普通光源 SSBO
    create_buffer(physical_device);

    VkDescriptorSetLayoutBinding bindings[3]{};
    for(uint32_t i=0;i<3;++i){
        bindings[i].binding=i;bindings[i].descriptorCount=1;
        bindings[i].descriptorType=i==1 ? VK_DESCRIPTOR_TYPE_STORAGE_BUFFER : VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        bindings[i].stageFlags=VK_SHADER_STAGE_FRAGMENT_BIT;
    }
    VkDescriptorSetLayoutCreateInfo layout{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO};
    layout.bindingCount=3;layout.pBindings=bindings;
    vkCreateDescriptorSetLayout(device,&layout,nullptr,&light_layout);
    VkDescriptorPoolSize sizes[]={{VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,2},{VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,1}};
    VkDescriptorPoolCreateInfo pool{VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO};
    pool.maxSets=1;pool.poolSizeCount=2;pool.pPoolSizes=sizes;
    vkCreateDescriptorPool(device,&pool,nullptr,&descriptor_pool);
    VkDescriptorSetAllocateInfo sets{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO};
    sets.descriptorPool=descriptor_pool;sets.descriptorSetCount=1;sets.pSetLayouts=&light_layout;
    vkAllocateDescriptorSets(device,&sets,&light_set);
    VkDescriptorBufferInfo buffers[]={{main_light_buffer,0,sizeof(main_light_uniform_data)},
        {light_buffer,0,sizeof(normal_light_uniform_data)*max_count},{light_count_buffer,0,sizeof(uint32_t)}};
    VkWriteDescriptorSet writes[3]{};
    for(uint32_t i=0;i<3;++i){
        writes[i].sType=VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;writes[i].dstSet=light_set;
        writes[i].dstBinding=i;writes[i].descriptorCount=1;writes[i].descriptorType=bindings[i].descriptorType;
        writes[i].pBufferInfo=&buffers[i];
    }
    vkUpdateDescriptorSets(device,3,writes,0,nullptr);
    light_datas.reserve(max_count);
}

void light_data::update_main_light() noexcept{
    std::memcpy(main_light_mapped,&main_light_data,sizeof(main_light_uniform_data));
}

void light_data::update_lights(VkPhysicalDevice physical_device){
    light_count = light_datas.size();

    //如果当前光源数量超过了 SSBO 的容量，则进行扩容
    if(light_count > max_count) resize_buffer(physical_device);
    if(light_count > 0) std::memcpy(light_mapped,light_datas.data(),sizeof(normal_light_uniform_data) * light_count);

    uint32_t light_count = static_cast<uint32_t>(light_datas.size());
    std::memcpy(light_count_mapped,&light_count,sizeof(uint32_t));
}

void light_data::update(VkPhysicalDevice physical_device){
    update_main_light();
    update_lights(physical_device);
}
void light_data::upload(VkCommandBuffer command_buffer,VkPipelineLayout pipeline_layout){
    vkCmdBindDescriptorSets(command_buffer,VK_PIPELINE_BIND_POINT_GRAPHICS,pipeline_layout,1,1,&light_set,0,nullptr);
}
void light_data::cleanup() noexcept{
    if(device == VK_NULL_HANDLE) return;
    //释放普通光源 SSBO
    if(light_mapped != nullptr){
        vkUnmapMemory(device,light_memory);
    }

    vkDestroyBuffer(device,light_buffer,nullptr);
    vkFreeMemory(device,light_memory,nullptr);

    //释放主光源 UBO
    if(main_light_mapped != nullptr){
        vkUnmapMemory(device,main_light_memory);
    }

    vkDestroyBuffer(device,main_light_buffer,nullptr);
    vkFreeMemory(device,main_light_memory,nullptr);
    if(light_count_mapped) vkUnmapMemory(device,light_count_memory);
    vkDestroyBuffer(device,light_count_buffer,nullptr);
    vkFreeMemory(device,light_count_memory,nullptr);
    vkDestroyDescriptorPool(device,descriptor_pool,nullptr);
    vkDestroyDescriptorSetLayout(device,light_layout,nullptr);
    device = VK_NULL_HANDLE;
}

}
