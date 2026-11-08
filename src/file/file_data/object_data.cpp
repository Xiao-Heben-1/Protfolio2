#include <file/file_data/object_data.hpp>
#include <file/file_scan.hpp>
#include <utility>

namespace render{
    void object_data::register_data(const std::string& uid){
        auto config = object_config::load(scan->get_source_path(uid));
        config.guid = uid;
        objects.insert_or_assign(uid,object_entry{std::move(config)});
    }
    void object_data::remove_data(const std::string& uid){objects.erase(uid);}
    void object_data::cleanup() noexcept{objects.clear();}
    object_config* object_data::get_object(const std::string& uid) noexcept{
        const auto it = objects.find(uid);
        return it == objects.end() ? nullptr : &it->second.config;
    }
    const object_config* object_data::get_object(const std::string& uid) const noexcept{
        const auto it = objects.find(uid);
        return it == objects.end() ? nullptr : &it->second.config;
    }
}


