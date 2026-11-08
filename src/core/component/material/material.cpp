#include <core/component/material/material.hpp>
#include <core/object/object.hpp>
#include <cstring>

namespace render{
material::material(object* owner):component(owner,3){
    if(owner != nullptr) owner->mat = this;
}

VkDescriptorSetLayout material::material_layout = VK_NULL_HANDLE;

void sub_material::init(VkDevice _device) noexcept{
    device = _device;
}

void sub_material::upload(VkCommandBuffer command_buffer,VkPipelineLayout pipeline_layout){
    std::memcpy(uniform_mapped,&uniform_data,sizeof(material_uniform_data));

    vkCmdBindDescriptorSets(command_buffer,VK_PIPELINE_BIND_POINT_GRAPHICS,pipeline_layout,2,1,&descriptor_set,0,nullptr);
}

void sub_material::cleanup() noexcept{
    if(device == VK_NULL_HANDLE) return;
    auto cleanup_texture = [&](material_texture& texture){
        vkDestroySampler(device,texture.sampler,nullptr);
        vkDestroyImageView(device,texture.view,nullptr);
        vkDestroyImage(device,texture.image,nullptr);
        vkFreeMemory(device,texture.memory,nullptr);
    };

    cleanup_texture(base_color_map);
    cleanup_texture(normal_map);
    cleanup_texture(metallic_map);
    cleanup_texture(roughness_map);
    cleanup_texture(ao_map);
    cleanup_texture(emissive_map);
    cleanup_texture(opacity_map);

    vkUnmapMemory(device,uniform_memory);
    vkDestroyBuffer(device,uniform_buffer,nullptr);
    vkFreeMemory(device,uniform_memory,nullptr);
    vkDestroyDescriptorPool(device,descriptor_pool,nullptr);
    device=VK_NULL_HANDLE;
}

sub_material::~sub_material(){
    cleanup();
}

void material::init(VkDevice _device) noexcept{
    component::init(_device);

    for(sub_material* mat : sub_materials){
        mat->init(_device);
    }
}

void material::upload(VkCommandBuffer command_buffer,VkPipelineLayout pipeline_layout){
    for(sub_material* mat : sub_materials){
        std::memcpy(mat->uniform_mapped,&mat->uniform_data,sizeof(material_uniform_data));
    }
}

void material::upload(VkCommandBuffer command_buffer,VkPipelineLayout pipeline_layout,uint32_t idx){
    sub_material* mat = sub_materials[idx];
    mat->upload(command_buffer,pipeline_layout);
}

void material::cleanup(){
    for(sub_material* mat : sub_materials){
        delete mat;
    }

    sub_materials.clear();
}


sub_material* material::add_sub_material() noexcept{
    sub_material* mat = new sub_material();
    mat->idx = sub_materials.size();
    sub_materials.push_back(mat);
    return mat;
}

void material::remove_sub_material(sub_material* mat) noexcept{
    uint32_t remove_idx = mat->idx;
    uint32_t last_idx = sub_materials.size() - 1;
    if(remove_idx != last_idx){
        sub_materials[remove_idx] = sub_materials[last_idx];
        sub_materials[remove_idx]->idx = remove_idx;
    }
    sub_materials.pop_back();

    delete mat;
}

material::~material(){
    cleanup();
}

}
