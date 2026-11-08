#include <file/file_data/scene_data.hpp>
#include <file/file_scan.hpp>
#include <utility>

namespace render{
    void scene_data::register_data(const std::string& uid){
        auto config = scene_config::load(scan->get_source_path(uid));
        config.guid = uid;
        scenes.insert_or_assign(uid,scene_entry{std::move(config)});
    }
    void scene_data::remove_data(const std::string& uid){scenes.erase(uid);}
    void scene_data::cleanup() noexcept{scenes.clear();}
    scene_config* scene_data::get_scene(const std::string& uid) noexcept{
        const auto it = scenes.find(uid);
        return it == scenes.end() ? nullptr : &it->second.config;
    }
    const scene_config* scene_data::get_scene(const std::string& uid) const noexcept{
        const auto it = scenes.find(uid);
        return it == scenes.end() ? nullptr : &it->second.config;
    }
}

