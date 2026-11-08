#include <core/render/data/camera_data.hpp>
#include <engine.hpp>
#include <core/component/component_enter.hpp>
#include <cstring>
#include <stdexcept>

namespace render{
void camera_data::init(VkDevice _device,VkExtent2D _extent,VkFormat _g_buffer_format,VkFormat _depth_format){
    gpu_data::init(_device);
    extent = _extent;
    g_buffer_format = _g_buffer_format;
    depth_format = _depth_format;
}
void camera_data::init(VkExtent2D _extent,VkFormat _g_buffer_format,VkFormat _depth_format){
    extent = _extent;
    g_buffer_format = _g_buffer_format;
    depth_format = _depth_format;
    
}
void camera_data::create(VkPhysicalDevice physical_device){
    auto create_image = [&](VkFormat format,VkImageUsageFlags usage,VkImage& image,VkDeviceMemory& memory){
        VkImageCreateInfo image_info{};
        image_info.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
        image_info.imageType = VK_IMAGE_TYPE_2D;
        image_info.extent = {extent.width,extent.height,1};
        image_info.mipLevels = 1;
        image_info.arrayLayers = 1;
        image_info.format = format;
        image_info.tiling = VK_IMAGE_TILING_OPTIMAL;
        image_info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        image_info.usage = usage;
        image_info.samples = VK_SAMPLE_COUNT_1_BIT;
        image_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

        vkCreateImage(device,&image_info,nullptr,&image);

        VkMemoryRequirements memory_requirements{};
        vkGetImageMemoryRequirements(device,image,&memory_requirements);

        VkMemoryAllocateInfo allocate_info{};
        allocate_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        allocate_info.allocationSize = memory_requirements.size;
        allocate_info.memoryTypeIndex = find_memory_type(physical_device,memory_requirements.memoryTypeBits,VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

        vkAllocateMemory(device,&allocate_info,nullptr,&memory);
        vkBindImageMemory(device,image,memory,0);
    };

    auto create_image_view = [&](VkImage image,VkFormat format,VkImageAspectFlags aspect,VkImageView& view){
        VkImageViewCreateInfo view_info{};
        view_info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        view_info.image = image;
        view_info.viewType = VK_IMAGE_VIEW_TYPE_2D;
        view_info.format = format;
        view_info.subresourceRange.aspectMask = aspect;
        view_info.subresourceRange.baseMipLevel = 0;
        view_info.subresourceRange.levelCount = 1;
        view_info.subresourceRange.baseArrayLayer = 0;
        view_info.subresourceRange.layerCount = 1;

        vkCreateImageView(device,&view_info,nullptr,&view);
    };

    auto create_sampler = [&](VkFilter filter,VkSampler& sampler){
        VkSamplerCreateInfo sampler_info{};
        sampler_info.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
        sampler_info.magFilter = filter;
        sampler_info.minFilter = filter;
        sampler_info.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
        sampler_info.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        sampler_info.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        sampler_info.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        sampler_info.minLod = 0.0f;
        sampler_info.maxLod = 0.0f;

        vkCreateSampler(device,&sampler_info,nullptr,&sampler);
    };

    //camera UBO
    VkBufferCreateInfo buffer_info{};
    buffer_info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    buffer_info.size = sizeof(camera_uniform_data);
    buffer_info.usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
    buffer_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    vkCreateBuffer(device,&buffer_info,nullptr,&uniform_buffer);

    VkMemoryRequirements memory_requirements{};
    vkGetBufferMemoryRequirements(device,uniform_buffer,&memory_requirements);

    VkMemoryAllocateInfo allocate_info{};
    allocate_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocate_info.allocationSize = memory_requirements.size;
    allocate_info.memoryTypeIndex = find_memory_type(physical_device,memory_requirements.memoryTypeBits,VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

    vkAllocateMemory(device,&allocate_info,nullptr,&uniform_memory);
    vkBindBufferMemory(device,uniform_buffer,uniform_memory,0);
    vkMapMemory(device,uniform_memory,0,sizeof(camera_uniform_data),0,&uniform_mapped);

    //颜色纹理
    create_image(g_buffer_format,VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,color_image,color_memory);
    create_image_view(color_image,g_buffer_format,VK_IMAGE_ASPECT_COLOR_BIT,color_view);
    create_sampler(VK_FILTER_LINEAR,color_sampler);
    create_image(VK_FORMAT_R8G8B8A8_UNORM,VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT|VK_IMAGE_USAGE_SAMPLED_BIT,display_image,display_memory);
    create_image_view(display_image,VK_FORMAT_R8G8B8A8_UNORM,VK_IMAGE_ASPECT_COLOR_BIT,display_view);
    display_initialized=false;

    //法线纹理
    create_image(g_buffer_format,VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,normal_image,normal_memory);
    create_image_view(normal_image,g_buffer_format,VK_IMAGE_ASPECT_COLOR_BIT,normal_view);
    create_sampler(VK_FILTER_NEAREST,normal_sampler);

    //深度纹理
    create_image(depth_format,VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,depth_image,depth_memory);
    create_image_view(depth_image,depth_format,VK_IMAGE_ASPECT_DEPTH_BIT,depth_view);
    create_sampler(VK_FILTER_NEAREST,depth_sampler);

    //camera layout
    VkDescriptorSetLayoutBinding camera_binding{};
    camera_binding.binding = 0;
    camera_binding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    camera_binding.descriptorCount = 1;
    camera_binding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;

    VkDescriptorSetLayoutCreateInfo camera_layout_info{};
    camera_layout_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    camera_layout_info.bindingCount = 1;
    camera_layout_info.pBindings = &camera_binding;

    if(!camera_layout)vkCreateDescriptorSetLayout(device,&camera_layout_info,nullptr,&camera_layout);

    //创建 G-Buffer layout
    VkDescriptorSetLayoutBinding g_buffer_bindings[3]{};

    g_buffer_bindings[0].binding = 0;//color
    g_buffer_bindings[0].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    g_buffer_bindings[0].descriptorCount = 1;
    g_buffer_bindings[0].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

    g_buffer_bindings[1].binding = 1;//normal
    g_buffer_bindings[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    g_buffer_bindings[1].descriptorCount = 1;
    g_buffer_bindings[1].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

    g_buffer_bindings[2].binding = 2;//depth
    g_buffer_bindings[2].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    g_buffer_bindings[2].descriptorCount = 1;
    g_buffer_bindings[2].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

    VkDescriptorSetLayoutCreateInfo g_buffer_layout_info{};
    g_buffer_layout_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    g_buffer_layout_info.bindingCount = 3;
    g_buffer_layout_info.pBindings = g_buffer_bindings;

    if(!g_buffer_layout)vkCreateDescriptorSetLayout(device,&g_buffer_layout_info,nullptr,&g_buffer_layout);

    //descriptor pool
    VkDescriptorPoolSize pool_sizes[2]{};

    pool_sizes[0].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    pool_sizes[0].descriptorCount = 1;

    pool_sizes[1].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    pool_sizes[1].descriptorCount = 3;

    VkDescriptorPoolCreateInfo pool_info{};
    pool_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    pool_info.poolSizeCount = 2;
    pool_info.pPoolSizes = pool_sizes;
    pool_info.maxSets = 4;

    vkCreateDescriptorPool(device,&pool_info,nullptr,&descriptor_pool);

    //descriptor sets
    VkDescriptorSetLayout layouts[2]{camera_layout,g_buffer_layout};
    VkDescriptorSet sets[2]{};

    VkDescriptorSetAllocateInfo descriptor_allocate_info{};
    descriptor_allocate_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    descriptor_allocate_info.descriptorPool = descriptor_pool;
    descriptor_allocate_info.descriptorSetCount = 2;
    descriptor_allocate_info.pSetLayouts = layouts;

    vkAllocateDescriptorSets(device,&descriptor_allocate_info,sets);

    camera_set = sets[0];
    g_buffer_set = sets[1];

    VkDescriptorBufferInfo camera_buffer_info{};
    camera_buffer_info.buffer = uniform_buffer;
    camera_buffer_info.offset = 0;
    camera_buffer_info.range = sizeof(camera_uniform_data);

    VkDescriptorImageInfo color_image_info{};
    color_image_info.sampler = color_sampler;
    color_image_info.imageView = color_view;
    color_image_info.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

    VkDescriptorImageInfo normal_image_info{};
    normal_image_info.sampler = normal_sampler;
    normal_image_info.imageView = normal_view;
    normal_image_info.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

    VkDescriptorImageInfo depth_image_info{};
    depth_image_info.sampler = depth_sampler;
    depth_image_info.imageView = depth_view;
    depth_image_info.imageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;

    VkWriteDescriptorSet writes[4]{};

    writes[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writes[0].dstSet = camera_set;
    writes[0].dstBinding = 0;
    writes[0].descriptorCount = 1;
    writes[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    writes[0].pBufferInfo = &camera_buffer_info;

    writes[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writes[1].dstSet = g_buffer_set;
    writes[1].dstBinding = 0;
    writes[1].descriptorCount = 1;
    writes[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    writes[1].pImageInfo = &color_image_info;

    writes[2].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writes[2].dstSet = g_buffer_set;
    writes[2].dstBinding = 1;
    writes[2].descriptorCount = 1;
    writes[2].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    writes[2].pImageInfo = &normal_image_info;

    writes[3].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writes[3].dstSet = g_buffer_set;
    writes[3].dstBinding = 2;
    writes[3].descriptorCount = 1;
    writes[3].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    writes[3].pImageInfo = &depth_image_info;

    vkUpdateDescriptorSets(device,4,writes,0,nullptr);
}

void camera_data::update(VkPhysicalDevice physical_device){
    uniform_data.view = egn.get_render_system().get_scene().get_main_camera().get_view();
    uniform_data.projection = egn.get_render_system().get_scene().get_main_camera().get_projection();
    uniform_data.vp = egn.get_render_system().get_scene().get_main_camera().get_vp();

    std::memcpy(uniform_mapped,&uniform_data,sizeof(camera_uniform_data));
}
void camera_data::upload(VkCommandBuffer command_buffer,VkPipelineLayout pipeline_layout){
    vkCmdBindDescriptorSets(command_buffer,VK_PIPELINE_BIND_POINT_GRAPHICS,pipeline_layout,0,1,&camera_set,0,nullptr);
    vkCmdBindDescriptorSets(command_buffer,VK_PIPELINE_BIND_POINT_GRAPHICS,pipeline_layout,3,1,&g_buffer_set,0,nullptr);
}

void camera_data::cleanup() noexcept{
    if(device == VK_NULL_HANDLE) return;
    //释放深度纹理
    vkDestroySampler(device,depth_sampler,nullptr);
    vkDestroyImageView(device,depth_view,nullptr);
    vkDestroyImage(device,depth_image,nullptr);
    vkFreeMemory(device,depth_memory,nullptr);

    //释放法线纹理
    vkDestroySampler(device,normal_sampler,nullptr);
    vkDestroyImageView(device,normal_view,nullptr);
    vkDestroyImage(device,normal_image,nullptr);
    vkFreeMemory(device,normal_memory,nullptr);

    vkDestroyImageView(device,display_view,nullptr);vkDestroyImage(device,display_image,nullptr);vkFreeMemory(device,display_memory,nullptr);
    //释放颜色纹理
    vkDestroySampler(device,color_sampler,nullptr);
    vkDestroyImageView(device,color_view,nullptr);
    vkDestroyImage(device,color_image,nullptr);
    vkFreeMemory(device,color_memory,nullptr);

    //释放 camera UBO
    if(uniform_mapped != nullptr){
        vkUnmapMemory(device,uniform_memory);
    }

    vkDestroyBuffer(device,uniform_buffer,nullptr);
    vkFreeMemory(device,uniform_memory,nullptr);

    //释放 descriptor
    vkDestroyDescriptorPool(device,descriptor_pool,nullptr);
    vkDestroyDescriptorSetLayout(device,camera_layout,nullptr);
    vkDestroyDescriptorSetLayout(device,g_buffer_layout,nullptr);
    camera_layout=g_buffer_layout=VK_NULL_HANDLE;
    device = VK_NULL_HANDLE;
}

void camera_data::resize(VkPhysicalDevice physical,VkExtent2D size){
    const auto logical=device;const auto camera_descriptor_layout=camera_layout,gbuffer_descriptor_layout=g_buffer_layout;
    camera_layout=g_buffer_layout=VK_NULL_HANDLE;
    cleanup();
    device=logical;extent=size;camera_layout=camera_descriptor_layout;g_buffer_layout=gbuffer_descriptor_layout;
    create(physical);
}

}
