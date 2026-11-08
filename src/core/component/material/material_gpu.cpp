#include <core/component/material/material.hpp>
#include <array>
#include <engine.hpp>
#include <cstring>
namespace render {
namespace {
uint32_t memory_type(VkPhysicalDevice physical,uint32_t bits,VkMemoryPropertyFlags flags){
    VkPhysicalDeviceMemoryProperties memory{};vkGetPhysicalDeviceMemoryProperties(physical,&memory);
    for(uint32_t i=0;i<memory.memoryTypeCount;++i)if((bits&(1u<<i)) && (memory.memoryTypes[i].propertyFlags&flags)==flags)return i;
    return 0;
}
}
void material::create_layout(VkDevice device){
    std::array<VkDescriptorSetLayoutBinding,8> bindings{};
    for(uint32_t i=0;i<8;++i)bindings[i]={i,i==0 ? VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER : VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,1,VK_SHADER_STAGE_FRAGMENT_BIT,nullptr};
    VkDescriptorSetLayoutCreateInfo info{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO};
    info.bindingCount=uint32_t(bindings.size());info.pBindings=bindings.data();vkCreateDescriptorSetLayout(device,&info,nullptr,&material_layout);
}
void material::destroy_layout(VkDevice device){vkDestroyDescriptorSetLayout(device,material_layout,nullptr);material_layout=VK_NULL_HANDLE;}
void material::prepare(VkPhysicalDevice physical,VkDevice logical,VkCommandPool pool,VkQueue queue){
    device=logical;
    for(auto* sub:sub_materials){
        const std::array<std::string,7> images{sub->base_color_map.image_uid,sub->normal_map.image_uid,sub->metallic_map.image_uid,
            sub->roughness_map.image_uid,sub->ao_map.image_uid,sub->emissive_map.image_uid,sub->opacity_map.image_uid};
        if(sub->device && sub->uploaded_images==images)continue;
        sub->cleanup();sub->create(physical,logical,pool,queue,material_layout);sub->uploaded_images=images;
    }
}
void sub_material::create(VkPhysicalDevice physical,VkDevice logical,VkCommandPool pool,VkQueue queue,VkDescriptorSetLayout layout){
    device=logical;
    VkBufferCreateInfo buffer{VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};buffer.size=sizeof(uniform_data);buffer.usage=VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
    vkCreateBuffer(device,&buffer,nullptr,&uniform_buffer);
    VkMemoryRequirements req{};vkGetBufferMemoryRequirements(device,uniform_buffer,&req);
    VkMemoryAllocateInfo memory{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};memory.allocationSize=req.size;
    memory.memoryTypeIndex=memory_type(physical,req.memoryTypeBits,VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT|VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    vkAllocateMemory(device,&memory,nullptr,&uniform_memory);vkBindBufferMemory(device,uniform_buffer,uniform_memory,0);
    vkMapMemory(device,uniform_memory,0,sizeof(uniform_data),0,&uniform_mapped);
    VkCommandBufferAllocateInfo allocation{VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};allocation.commandPool=pool;allocation.level=VK_COMMAND_BUFFER_LEVEL_PRIMARY;allocation.commandBufferCount=1;
    VkCommandBuffer command;vkAllocateCommandBuffers(device,&allocation,&command);
    VkCommandBufferBeginInfo begin{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};begin.flags=VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;vkBeginCommandBuffer(command,&begin);
    std::array<material_texture*,7> textures{&base_color_map,&normal_map,&metallic_map,&roughness_map,&ao_map,&emissive_map,&opacity_map};
    std::array<VkDescriptorImageInfo,7> images{};
    std::vector<std::pair<VkBuffer,VkDeviceMemory>> staging;
    for(size_t i=0;i<textures.size();++i){
        auto& texture=*textures[i];
        const image_data::image_entry* source=texture.image_uid.empty() ? nullptr : &egn.get_file_system().get_resources().get_image_data().get_image(texture.image_uid);
        const uint32_t levels=source ? source->config.mip_levels : 1;
        VkImageCreateInfo image{VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};image.imageType=VK_IMAGE_TYPE_2D;image.format=VK_FORMAT_R8G8B8A8_UNORM;
        image.extent={source ? source->config.width : 1,source ? source->config.height : 1,1};
        if(source){const bool alpha=source->config.resolved_format=="bc3",srgb=i==0 || i==5;
            image.format=alpha ? (srgb ? VK_FORMAT_BC3_SRGB_BLOCK : VK_FORMAT_BC3_UNORM_BLOCK) : (srgb ? VK_FORMAT_BC1_RGBA_SRGB_BLOCK : VK_FORMAT_BC1_RGBA_UNORM_BLOCK);}
        image.mipLevels=levels;image.arrayLayers=1;image.samples=VK_SAMPLE_COUNT_1_BIT;image.tiling=VK_IMAGE_TILING_OPTIMAL;
        texture.extent={image.extent.width,image.extent.height};texture.format=image.format;
        image.usage=VK_IMAGE_USAGE_SAMPLED_BIT|VK_IMAGE_USAGE_TRANSFER_DST_BIT;
        vkCreateImage(device,&image,nullptr,&texture.image);vkGetImageMemoryRequirements(device,texture.image,&req);
        memory.allocationSize=req.size;memory.memoryTypeIndex=memory_type(physical,req.memoryTypeBits,VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
        vkAllocateMemory(device,&memory,nullptr,&texture.memory);vkBindImageMemory(device,texture.image,texture.memory,0);
        VkImageMemoryBarrier barrier{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};barrier.image=texture.image;barrier.newLayout=VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        barrier.srcQueueFamilyIndex=barrier.dstQueueFamilyIndex=VK_QUEUE_FAMILY_IGNORED;
        barrier.subresourceRange={VK_IMAGE_ASPECT_COLOR_BIT,0,levels,0,1};barrier.dstAccessMask=VK_ACCESS_TRANSFER_WRITE_BIT;
        vkCmdPipelineBarrier(command,VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,VK_PIPELINE_STAGE_TRANSFER_BIT,0,0,nullptr,0,nullptr,1,&barrier);
        if(source){
            VkBuffer upload;VkDeviceMemory upload_memory;
            VkBufferCreateInfo info{VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};info.size=source->dds.size()-148;info.usage=VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
            vkCreateBuffer(device,&info,nullptr,&upload);vkGetBufferMemoryRequirements(device,upload,&req);
            memory.allocationSize=req.size;memory.memoryTypeIndex=memory_type(physical,req.memoryTypeBits,VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT|VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
            vkAllocateMemory(device,&memory,nullptr,&upload_memory);vkBindBufferMemory(device,upload,upload_memory,0);
            void* mapped;vkMapMemory(device,upload_memory,0,info.size,0,&mapped);std::memcpy(mapped,source->dds.data()+148,size_t(info.size));vkUnmapMemory(device,upload_memory);
            std::vector<VkBufferImageCopy> copies;VkDeviceSize offset=0;uint32_t w=image.extent.width,h=image.extent.height;
            for(uint32_t mip=0;mip<levels;++mip){VkBufferImageCopy copy{};copy.bufferOffset=offset;copy.imageSubresource={VK_IMAGE_ASPECT_COLOR_BIT,mip,0,1};copy.imageExtent={w,h,1};copies.push_back(copy);
                offset+=VkDeviceSize((w+3)/4)*((h+3)/4)*(source->config.resolved_format=="bc3" ? 16 : 8);w=std::max(1u,w/2);h=std::max(1u,h/2);}
            vkCmdCopyBufferToImage(command,upload,texture.image,VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,uint32_t(copies.size()),copies.data());staging.emplace_back(upload,upload_memory);
        }else{
            VkClearColorValue color{{1,1,1,1}};if(i==1)color={{.5f,.5f,1,1}};
            vkCmdClearColorImage(command,texture.image,VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,&color,1,&barrier.subresourceRange);
        }
        barrier.oldLayout=VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;barrier.newLayout=VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        barrier.srcAccessMask=VK_ACCESS_TRANSFER_WRITE_BIT;barrier.dstAccessMask=VK_ACCESS_SHADER_READ_BIT;
        vkCmdPipelineBarrier(command,VK_PIPELINE_STAGE_TRANSFER_BIT,VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,0,0,nullptr,0,nullptr,1,&barrier);
        VkImageViewCreateInfo view{VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};view.image=texture.image;view.viewType=VK_IMAGE_VIEW_TYPE_2D;
        view.format=image.format;view.subresourceRange=barrier.subresourceRange;vkCreateImageView(device,&view,nullptr,&texture.view);
        VkSamplerCreateInfo sampler{VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO};sampler.magFilter=sampler.minFilter=VK_FILTER_LINEAR;
        sampler.mipmapMode=VK_SAMPLER_MIPMAP_MODE_LINEAR;sampler.maxLod=float(levels-1);
        sampler.addressModeU=sampler.addressModeV=sampler.addressModeW=VK_SAMPLER_ADDRESS_MODE_REPEAT;
        vkCreateSampler(device,&sampler,nullptr,&texture.sampler);
        images[i]={texture.sampler,texture.view,VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL};
    }
    vkEndCommandBuffer(command);VkSubmitInfo submit{VK_STRUCTURE_TYPE_SUBMIT_INFO};submit.commandBufferCount=1;submit.pCommandBuffers=&command;
    vkQueueSubmit(queue,1,&submit,VK_NULL_HANDLE);vkQueueWaitIdle(queue);vkFreeCommandBuffers(device,pool,1,&command);
    for(const auto& [buffer,memory]:staging){vkDestroyBuffer(device,buffer,nullptr);vkFreeMemory(device,memory,nullptr);}
    VkDescriptorPoolSize sizes[]={{VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,1},{VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,7}};
    VkDescriptorPoolCreateInfo pool_info{VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO};pool_info.maxSets=1;pool_info.poolSizeCount=2;pool_info.pPoolSizes=sizes;
    vkCreateDescriptorPool(device,&pool_info,nullptr,&descriptor_pool);
    VkDescriptorSetAllocateInfo sets{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO};sets.descriptorPool=descriptor_pool;sets.descriptorSetCount=1;sets.pSetLayouts=&layout;
    vkAllocateDescriptorSets(device,&sets,&descriptor_set);
    VkDescriptorBufferInfo ubo{uniform_buffer,0,sizeof(uniform_data)};
    std::array<VkWriteDescriptorSet,8> writes{};
    for(uint32_t i=0;i<8;++i){auto& write=writes[i];write.sType=VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;write.dstSet=descriptor_set;write.dstBinding=i;write.descriptorCount=1;
        write.descriptorType=i==0 ? VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER : VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        if(i==0)write.pBufferInfo=&ubo;else write.pImageInfo=&images[i-1];
    }
    vkUpdateDescriptorSets(device,uint32_t(writes.size()),writes.data(),0,nullptr);
}
}
