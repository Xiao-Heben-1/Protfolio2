#include <file/file_data/image_data.hpp>
#include <file/file_scan.hpp>
#include <file/tools/library_cache.hpp>

namespace render{
    void image_data::register_library(const std::string& uid){
        // 配置路径来自 scan，data 只保留压缩选项和内存数据。
        auto config = texture_config::load(scan->get_library_path(uid).string() + ".texture_config");
        images[uid] = image_entry{std::move(config),{}};
    }
    void image_data::remove_data(const std::string& uid){
        images.erase(uid);
    }
    const image_data::image_entry& image_data::get_image(const std::string& uid){
        auto& entry = images.at(uid);
        // 第一次使用才读入 DDS；重新注册后 dds 为空，会加载最新文件。
        if(entry.dds.empty()) entry.dds = library_cache::read(scan->get_library_path(uid));
        return entry;
    }
}
