#pragma once
#include <file/config/file_config.hpp>
#include <file/config/object_config.hpp>
#include <file/config/component/camera_config.hpp>
#include <file/config/component/light_config.hpp>
#include <file/config/skybox_config.hpp>
#include <filesystem>
#include <vector>
namespace render {
// 场景只组织主相机、灯光和对象，组件字段及其序列化位于各自配置文件。
struct scene_config final:public file_config {
    camera_config main_camera;
    skybox_config skybox;
    std::vector<light_config> lights;
    std::vector<object_config> objects;
    static scene_config load(const std::filesystem::path& path);
private:
    friend class file_scan;
    void save(const std::filesystem::path& path) const;
};
}
