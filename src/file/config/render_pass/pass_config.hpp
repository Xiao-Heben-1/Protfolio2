#pragma once
#include <file/config/file_config.hpp>
#include <filesystem>
#include <vector>
#include <vulkan/vulkan.h>
namespace render {
enum class pass_stage { pre_processing, opaque, transparent_capture, transparent_sort, transparent_resolve, post_processing, geometry, skybox };
enum class pass_kind { graphics, compute };
struct pass_blend_config {
    bool enabled = false;
    VkBlendFactor src_color = VK_BLEND_FACTOR_ONE, dst_color = VK_BLEND_FACTOR_ZERO;
    VkBlendOp color_op = VK_BLEND_OP_ADD;
    VkBlendFactor src_alpha = VK_BLEND_FACTOR_ONE, dst_alpha = VK_BLEND_FACTOR_ZERO;
    VkBlendOp alpha_op = VK_BLEND_OP_ADD;
    VkColorComponentFlags write_mask = 15;
};
struct pass_config final:public file_config {
    pass_stage stage = pass_stage::opaque;
    pass_kind kind = pass_kind::graphics;
    uint32_t order = 0; // 同一阶段内的顺序
    bool enabled = true;
    std::string shader_guid;
    std::string builtin; // 内置阶段键；空为自定义阶段。
    VkPrimitiveTopology topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    VkPolygonMode polygon_mode = VK_POLYGON_MODE_FILL;
    VkCullModeFlags cull_mode = VK_CULL_MODE_NONE;
    VkFrontFace front_face = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    bool depth_test = true, depth_write = true;
    VkCompareOp depth_compare = VK_COMPARE_OP_LESS;
    std::vector<pass_blend_config> color_blends; // 按颜色附件顺序配置；compute 不使用图形状态
    static pass_config load(const std::filesystem::path& path);
private:
    friend class file_scan;
    void save(const std::filesystem::path& path) const;
};
}
