#pragma once
#include <array>
#include <optional>
#include <string>
#include <tools/structures/AABB.hpp>
#include <rapidjson/document.h>
namespace render {
struct mesh_config {
    std::string builtin_geometry; // 系统几何类型；可在清空 library 后重新生成。
    std::string source_guid; // 原始模型文件。
    std::array<std::string,3> lod_guids; // LOD0/1/2几何资源；没有LOD时只填第0项。
    std::optional<AABB> bounds; // 模型局部AABB；未指定时读取资源文件头。
private:
    friend struct object_config;
    rapidjson::Value write(rapidjson::Document::AllocatorType& alloc) const;
    void read(const rapidjson::Value& source);
};
}
