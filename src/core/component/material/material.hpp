#ifndef __RENDER_MATERIAL_HPP
#define __RENDER_MATERIAL_HPP

#include <cstdint>
#include <vector>
#include <array>
#include <glm/glm.hpp>
#include <vulkan/vulkan.h>
#include <core/component/component.hpp>
#include <core/component/material/shader/shader.hpp>
#include <core/component/material/texture/texture.hpp>
namespace render{
namespace front {class editor;}

/// @brief PBR 材质 Uniform 数据
struct material_uniform_data final{
    glm::vec4 base_color{1.0f,1.0f,1.0f,1.0f};//基础颜色 RGBA
    glm::vec4 emissive_color{0.0f,0.0f,0.0f,1.0f};//自发光颜色
    glm::vec4 parameter{0.0f,0.5f,1.0f,1.0f};//x metallic，y roughness，z ao，w emissive_strength
    glm::uvec4 texture_flags{0,0,0,0};//x base_color_map，y normal_map，z metallic_map，w roughness_map
    glm::uvec4 texture_flags_2{0,0,0,0};//x ao，y emissive，z alpha模式(0自动/1不透明/2裁剪/3混合)，w cutoff浮点位
    glm::uvec4 texture_channels{0,0,0,0}; // metallic通道、roughness通道、opacity启用、opacity通道。
    glm::uvec4 alpha_options{1,0,0,0}; // x：基础贴图 Alpha 是否表示不透明度；游戏遮罩贴图设为 0。
};

class object;
/// @brief 单个 PBR 子材质
class sub_material final{
    friend class material;
    friend class object_loader;
public:
    std::string name;
    std::string shader_guid;
    bool visible=true; // 控制使用该材质槽的子网格，不删除源模型中的几何。
    material_uniform_data uniform_data;//材质基础参数

    material_texture base_color_map;//基础颜色贴图：每像素基础颜色/albedo
    material_texture normal_map;//法线贴图：每像素 tangent space 法线方向
    material_texture metallic_map;//金属度贴图：0 非金属，1 金属
    material_texture roughness_map;//粗糙度贴图：0 光滑，1 粗糙
    material_texture ao_map;//环境遮蔽贴图：1 无遮蔽，0 强遮蔽
    material_texture emissive_map;//自发光贴图：控制表面哪些区域自行发光
    material_texture opacity_map;

    VkBuffer uniform_buffer = VK_NULL_HANDLE;//材质参数 UBO
    VkDeviceMemory uniform_memory = VK_NULL_HANDLE;//材质参数 UBO 显存
    void* uniform_mapped = nullptr;//材质参数 UBO 映射地址

    VkDescriptorSet descriptor_set = VK_NULL_HANDLE;//材质 descriptor set
protected:
    void upload(VkCommandBuffer command_buffer,VkPipelineLayout pipeline_layout);//绑定该子材质
private:
    VkDevice device = VK_NULL_HANDLE;
    VkDescriptorPool descriptor_pool = VK_NULL_HANDLE;
    std::array<std::string,7> uploaded_images;
    uint32_t idx = 0;//在 material 中的索引
    void init(VkDevice device) noexcept;
    void create(VkPhysicalDevice physical,VkDevice device,VkCommandPool pool,VkQueue queue,VkDescriptorSetLayout layout);
    void cleanup() noexcept;
    sub_material() = default;
    ~sub_material();
};

class render_system;
/// @brief object 的材质组件，管理所有 sub_material
class material final:public component{
    friend class front::editor;
    friend class object;
    friend class object_loader;
    friend class render_system;
private:
    std::vector<sub_material*> sub_materials;//模型包含的所有子材质
protected:
    static VkDescriptorSetLayout material_layout;//所有材质共用的 descriptor layout
public:
    const std::vector<sub_material*>& get_sub_materials() const noexcept{
        return sub_materials;
    }

    sub_material* get_sub_material(uint32_t idx) const noexcept{
        return sub_materials[idx];
    }

protected:
    void init(VkDevice _device) noexcept override;
    void upload(VkCommandBuffer command_buffer,VkPipelineLayout pipeline_layout) override;
    void upload(VkCommandBuffer command_buffer,VkPipelineLayout pipeline_layout,uint32_t idx);
    void cleanup() override;
private:
    explicit material(object* owner);
    ~material();
    static void create_layout(VkDevice device);
    static void destroy_layout(VkDevice device);
    void prepare(VkPhysicalDevice physical,VkDevice device,VkCommandPool pool,VkQueue queue);

    sub_material* add_sub_material() noexcept;//创建子材质
    void remove_sub_material(sub_material* mat) noexcept;//删除子材质
};
}
#endif // __RENDER_MATERIAL_HPP
