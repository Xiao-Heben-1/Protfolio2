#pragma once
#include <array>
#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <vector>
#include <rapidjson/document.h>
namespace render {
// 未填写的字段继承材质资源，slot对应sub_mesh::mat_idx。
struct material_override_config {
    uint32_t slot=0;
    std::optional<std::array<float,4>> base_color,emissive_color;
    std::optional<float> metallic,roughness,ao,emissive_strength;
    std::optional<uint32_t> alpha_mode; // 0自动、1不透明、2裁剪、3混合。
    std::optional<float> alpha_cutoff;
    std::optional<bool> base_color_alpha; // 基础贴图 Alpha 表示不透明度，而非独立材质遮罩。
    std::optional<bool> visible; // 使用该槽的子网格是否参与绘制。
    std::optional<std::array<uint32_t,4>> texture_channels;
    std::optional<std::string> shader_guid;
    // base_color/normal/metallic/roughness/ao/emissive -> 贴图GUID。
    std::map<std::string,std::string> textures;
private:
    friend struct material_config;
    rapidjson::Value write(rapidjson::Document::AllocatorType& alloc) const;
    void read(const rapidjson::Value& source);
};
struct material_config {
    std::vector<std::string> guids; // 按材质槽顺序排列。
    std::vector<material_override_config> overrides;
private:
    friend struct object_config;
    rapidjson::Value write(rapidjson::Document::AllocatorType& alloc) const;
    void read(const rapidjson::Value& source);
    void read_legacy(const rapidjson::Value& source);
};
}
