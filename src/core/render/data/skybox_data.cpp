#include <core/render/data/skybox_data.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <cstring>
namespace render {
namespace {
// vec4 显式打包，禁止直接复制含虚表的 skybox_data，避免 vec3 的 std140 对齐歧义。
struct alignas(16) atmosphere_uniform {
    glm::vec4 planet; // 半径、高度、Rayleigh/Mie尺度高度，m。
    glm::vec4 rayleigh; // RGB散射系数，w 密度倍率。
    glm::vec4 mie; // RGB散射系数，w 相函数 g。
    glm::vec4 extinction; // RGB米氏消光，w 曝光。
    glm::vec4 ozone; // RGB吸收系数，w 中心高度。
    glm::vec4 ground_albedo; // RGB平均地表反照率，w 反射倍率。
    glm::vec4 ground_color; // RGB可见地面底色，w 臭氧半宽。
    glm::ivec4 samples; // x视线积分，y光线积分；zw保留。
};
static_assert(sizeof(atmosphere_uniform)==128);
}
skybox_data::~skybox_data(){cleanup();}
VkDescriptorSetLayout skybox_data::get_layout() const{return layout;}
skybox_config skybox_data::get_config() const{
    skybox_config config;
    config.earth_radius=earth_radius;
    config.atmosphere_height=atmosphere_height;
    config.rayleigh_scale_height=rayleigh_scale_height;
    config.mie_scale_height=mie_scale_height;
    config.rayleigh_scattering={rayleigh_scattering.x,rayleigh_scattering.y,rayleigh_scattering.z};
    config.mie_scattering={mie_scattering.x,mie_scattering.y,mie_scattering.z};
    config.mie_extinction={mie_extinction.x,mie_extinction.y,mie_extinction.z};
    config.mie_g=mie_g;
    config.ozone_absorption={ozone_absorption.x,ozone_absorption.y,ozone_absorption.z};
    config.ozone_center_height=ozone_center_height;
    config.ozone_half_width=ozone_half_width;
    config.exposure=exposure;
    config.ground_albedo={ground_albedo.x,ground_albedo.y,ground_albedo.z};
    config.ground_intensity=ground_intensity;
    config.ground_color={ground_color.x,ground_color.y,ground_color.z};
    config.atmosphere_density=atmosphere_density;
    config.view_sample_count=view_sample_count;
    config.light_sample_count=light_sample_count;
    return config;
}
void skybox_data::apply_config(const skybox_config& config){
    earth_radius=config.earth_radius;
    atmosphere_height=config.atmosphere_height;
    rayleigh_scale_height=config.rayleigh_scale_height;
    mie_scale_height=config.mie_scale_height;
    rayleigh_scattering=glm::make_vec3(config.rayleigh_scattering.data());
    mie_scattering=glm::make_vec3(config.mie_scattering.data());
    mie_extinction=glm::make_vec3(config.mie_extinction.data());
    mie_g=config.mie_g;
    ozone_absorption=glm::make_vec3(config.ozone_absorption.data());
    ozone_center_height=config.ozone_center_height;
    ozone_half_width=config.ozone_half_width;
    exposure=config.exposure;
    ground_albedo=glm::make_vec3(config.ground_albedo.data());
    ground_intensity=config.ground_intensity;
    ground_color=glm::make_vec3(config.ground_color.data());
    atmosphere_density=config.atmosphere_density;
    view_sample_count=config.view_sample_count;
    light_sample_count=config.light_sample_count;
}
void skybox_data::create(VkPhysicalDevice physical){
    VkBufferCreateInfo info{VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};info.size=sizeof(atmosphere_uniform);info.usage=VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
    vkCreateBuffer(device,&info,nullptr,&buffer);
    VkMemoryRequirements requirements;vkGetBufferMemoryRequirements(device,buffer,&requirements);
    VkMemoryAllocateInfo allocate{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};allocate.allocationSize=requirements.size;
    allocate.memoryTypeIndex=find_memory_type(physical,requirements.memoryTypeBits,VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT|VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    vkAllocateMemory(device,&allocate,nullptr,&memory);vkBindBufferMemory(device,buffer,memory,0);
    vkMapMemory(device,memory,0,sizeof(atmosphere_uniform),0,&mapped);
    VkDescriptorSetLayoutBinding binding{0,VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,1,VK_SHADER_STAGE_FRAGMENT_BIT,nullptr};
    VkDescriptorSetLayoutCreateInfo layout_info{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO};layout_info.bindingCount=1;layout_info.pBindings=&binding;
    vkCreateDescriptorSetLayout(device,&layout_info,nullptr,&layout);
    VkDescriptorPoolSize size{VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,1};
    VkDescriptorPoolCreateInfo pool_info{VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO};pool_info.maxSets=1;pool_info.poolSizeCount=1;pool_info.pPoolSizes=&size;
    vkCreateDescriptorPool(device,&pool_info,nullptr,&pool);
    VkDescriptorSetAllocateInfo set{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO};set.descriptorPool=pool;set.descriptorSetCount=1;set.pSetLayouts=&layout;
    vkAllocateDescriptorSets(device,&set,&descriptor);
    VkDescriptorBufferInfo data{buffer,0,sizeof(atmosphere_uniform)};
    VkWriteDescriptorSet write{VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};write.dstSet=descriptor;write.dstBinding=0;write.descriptorCount=1;
    write.descriptorType=VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;write.pBufferInfo=&data;vkUpdateDescriptorSets(device,1,&write,0,nullptr);
    update(physical);
}
void skybox_data::update(VkPhysicalDevice){
    const atmosphere_uniform data{{earth_radius,atmosphere_height,rayleigh_scale_height,mie_scale_height},
        {rayleigh_scattering,atmosphere_density},{mie_scattering,mie_g},{mie_extinction,exposure},
        {ozone_absorption,ozone_center_height},{ground_albedo,ground_intensity},{ground_color,ozone_half_width},
        {view_sample_count,light_sample_count,0,0}};
    std::memcpy(mapped,&data,sizeof(data));
}
void skybox_data::upload(VkCommandBuffer command,VkPipelineLayout pipeline){
    vkCmdBindDescriptorSets(command,VK_PIPELINE_BIND_POINT_GRAPHICS,pipeline,5,1,&descriptor,0,nullptr);
}
void skybox_data::cleanup() noexcept{
    if(!device)return;
    if(mapped)vkUnmapMemory(device,memory);
    vkDestroyBuffer(device,buffer,nullptr);vkFreeMemory(device,memory,nullptr);
    vkDestroyDescriptorPool(device,pool,nullptr);vkDestroyDescriptorSetLayout(device,layout,nullptr);
    device=VK_NULL_HANDLE;mapped=nullptr;
}
}
