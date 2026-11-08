#pragma once
#include <filesystem>
#include <vector>
namespace render {
// 相对 workspace_root 以配置文件目录为基准；library_root 以 workspace_root 为基准。
struct workspace_config {
    std::filesystem::path workspace_root = ".";
    std::filesystem::path source_root = "."; // 唯一源资源扫描目录，相对工作空间；编辑器 workspace.json 默认 src。
    std::filesystem::path library_root = "library";
    std::filesystem::path engine_root; // 只读系统定义所在 render 根目录。
    std::filesystem::path builtin_shader_root = "builtin"; // 相对于 library_root，部署构建生成的内置 SPIR-V
    std::filesystem::path shader_compiler; // 留空使用 VULKAN_SDK/Bin/glslc.exe，或 PATH 中的 glslc.exe
    std::filesystem::path startup_scene = "generated/scenes/default.scene"; // 相对 library_root。
    std::vector<std::filesystem::path> shader_include_roots = {"shader/core/include","shader"};
    static workspace_config load(const std::filesystem::path& path);
    void save(const std::filesystem::path& path) const;
private:
    friend class file_system;
    void resolve(const std::filesystem::path& base);
};
}
