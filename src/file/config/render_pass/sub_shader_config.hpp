#pragma once
#include <file/config/file_config.hpp>
#include <filesystem>
#include <vector>
#include <map>
namespace render {
enum class shader_stage { vertex, fragment, geometry, tess_control, tess_evaluation, compute };
struct sub_shader_config final:public file_config {
    std::string source_guid;
    std::filesystem::path builtin_source; // 相对引擎 shader 目录，只读引用，不注册为工作空间资源。
    shader_stage stage = shader_stage::vertex;
    std::string entry_point = "main";
    std::map<std::string,std::string> defines;
    std::vector<std::filesystem::path> include_roots; // source_root 相对路径；追加到全局 include 路径前
    static sub_shader_config load(const std::filesystem::path& path);
private:
    friend class file_scan;
    void save(const std::filesystem::path& path) const;
};
}
