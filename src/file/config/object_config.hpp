#pragma once
#include <file/config/file_config.hpp>
#include <file/config/component/transform_config.hpp>
#include <file/config/component/mesh_config.hpp>
#include <file/config/component/material_config.hpp>
#include <file/config/component/camera_config.hpp>
#include <file/config/component/light_config.hpp>
#include <filesystem>
#include <optional>
namespace render {
// 对象持久化信息；运行时run_id、cur_lod及GPU句柄不写入配置。
struct object_config final:public file_config {
    bool is_transparent=false;
    bool behavior=false;
    transform_config transform;
    std::optional<mesh_config> mesh;
    std::optional<material_config> material;
    std::optional<camera_config> camera;
    std::vector<light_config> lights;
    static object_config load(const std::filesystem::path& path);
private:
    friend class file_scan;
    friend struct scene_config;
    rapidjson::Value write(rapidjson::Document::AllocatorType& alloc) const;
    void read(const rapidjson::Value& source);
    void save(const std::filesystem::path& path) const;
};
}
