#pragma once
#include <file/file_data/file_data.hpp>
#include <file/config/render_pass/pass_config.hpp>
#include <file/config/render_pass/shader_config.hpp>
#include <file/config/render_pass/sub_shader_config.hpp>
#include <unordered_map>
namespace render {
// 只缓存配置；GPU 管线仍由 render_system / spirv_data 管理。
class render_pass_data final:public file_data {
    friend class file_resources;
    std::unordered_map<std::string,pass_config> passes;
    std::unordered_map<std::string,shader_config> shaders;
    std::unordered_map<std::string,sub_shader_config> sub_shaders;
public:
    pass_config* get_pass(const std::string& guid);
    const pass_config* get_pass(const std::string& guid) const;
    shader_config* get_shader(const std::string& guid);
    const shader_config* get_shader(const std::string& guid) const;
    sub_shader_config* get_sub_shader(const std::string& guid);
    const sub_shader_config* get_sub_shader(const std::string& guid) const;
    std::vector<const pass_config*> get_passes(pass_stage stage) const; // 已启用，按 order / GUID 排序
private:
    render_pass_data():file_data(VK_NULL_HANDLE){}
    void register_data(const std::string& guid) override;
    void remove_data(const std::string& guid) override;
    void cleanup() noexcept override;
};
}
