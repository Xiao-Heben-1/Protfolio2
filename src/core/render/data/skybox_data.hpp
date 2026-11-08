#ifndef __RENDER_SKYBOX_DATA_HPP
#define __RENDER_SKYBOX_DATA_HPP
#include <core/render/data/data.hpp>
#include <glm/glm.hpp>
#include <file/config/skybox_config.hpp>
namespace render{
// 大气只保存介质/地表参数；太阳方向、辐照度和角半径由 main_light 统一提供。
struct skybox_data final:public gpu_data{
    float earth_radius = 6371000.0f;                  /// 地球半径，单位：m
    float atmosphere_height = 100000.0f;             /// 大气层高度，单位：m

    float rayleigh_scale_height = 8000.0f;            /// 瑞利散射密度尺度高度，空气分子密度随高度指数衰减，单位：m
    float mie_scale_height = 1200.0f;                 /// 米氏散射密度尺度高度，气溶胶密度随高度指数衰减，单位：m

    glm::vec3 rayleigh_scattering = glm::vec3(5.802e-6f,13.558e-6f,33.100e-6f); /// 瑞利散射系数，RGB分别对应红绿蓝波段，单位：1/m
    glm::vec3 mie_scattering = glm::vec3(3.996e-6f);  /// 米氏散射系数，控制气溶胶将光散射出去的强度，单位：1/m
    glm::vec3 mie_extinction = glm::vec3(4.440e-6f);  /// 米氏消光系数，包含散射和吸收导致的总衰减，单位：1/m

    float mie_g = 0.8f;                               /// 米氏散射相函数不对称参数，越接近1前向散射越集中

    glm::vec3 ozone_absorption = glm::vec3(0.650e-6f,1.881e-6f,0.085e-6f); /// 臭氧吸收系数，主要影响黄昏和高层大气颜色，单位：1/m
    float ozone_center_height = 25000.0f;             /// 臭氧层密度峰值高度，单位：m

    float ozone_half_width = 15000.0f;                /// 臭氧层近似分布的半宽度，单位：m
    float exposure = 1.0f;                            /// 天空与物体统一曝光倍率，透明合成之后应用

    glm::vec3 ground_albedo = glm::vec3(0.3f);        /// 地表平均反照率，用于地面反射以及地平线附近的间接贡献
    float ground_intensity = 1.0f;                    /// 地面反射贡献倍率

    glm::vec3 ground_color = glm::vec3(0.15f);        /// 视线击中地球时的线性RGB地面底色（漫反射率），与ground_albedo的平均间接反照率分开
    float atmosphere_density = 1.0f;                  /// 整体大气密度倍率，可用于调节大气浓厚程度

    int view_sample_count = 16;                       /// 沿相机视线积分时的采样次数
    int light_sample_count = 8;                       /// 每个视线采样点沿太阳方向计算光学深度时的采样次数
    ~skybox_data() override;
    skybox_config get_config() const;
    void apply_config(const skybox_config& config);
    void create(VkPhysicalDevice physical_device) override;
    void update(VkPhysicalDevice physical_device) override;
    void upload(VkCommandBuffer command_buffer,VkPipelineLayout pipeline_layout) override;
    void cleanup() noexcept override;
    VkDescriptorSetLayout get_layout() const;
private:
    VkBuffer buffer=VK_NULL_HANDLE;
    VkDeviceMemory memory=VK_NULL_HANDLE;
    void* mapped=nullptr;
    VkDescriptorPool pool=VK_NULL_HANDLE;
    VkDescriptorSetLayout layout=VK_NULL_HANDLE; // set 5, binding 0, std140。
    VkDescriptorSet descriptor=VK_NULL_HANDLE;

};
}
#endif
