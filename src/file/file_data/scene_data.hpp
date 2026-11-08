#ifndef __RENDER_SCENE_DATA_HPP
#define __RENDER_SCENE_DATA_HPP

#include <file/file_data/file_data.hpp>
#include <file/config/scene_config.hpp>
#include <unordered_map>

namespace render{
    class file_resources;
    // 场景配置缓存；读取配置不会实例化 render_scene 或分配 GPU 资源。
    class scene_data final:public file_data{
        friend class file_resources;
    public:
        scene_config* get_scene(const std::string& uid) noexcept;
        const scene_config* get_scene(const std::string& uid) const noexcept;
    private:
        struct scene_entry{
            scene_config config;
        };
        scene_data():file_data(VK_NULL_HANDLE){}
        void register_data(const std::string& uid) override;
        void register_library(const std::string&) override{}
        void remove_data(const std::string& uid) override;
        void cleanup() noexcept override;
        std::unordered_map<std::string,scene_entry> scenes;
    };
}

#endif // __RENDER_SCENE_DATA_HPP

