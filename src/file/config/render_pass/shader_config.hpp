#pragma once
#include <file/config/file_config.hpp>
#include <filesystem>
#include <vector>
namespace render {
struct shader_config final:public file_config {
    std::vector<std::string> sub_shader_guids;
    static shader_config load(const std::filesystem::path& path);
private:
    friend class file_scan;
    void save(const std::filesystem::path& path) const;
};
}
