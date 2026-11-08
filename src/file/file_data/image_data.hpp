#ifndef __RENDER_IMAGE_DATA_HPP
#define __RENDER_IMAGE_DATA_HPP
#include <file/file_data/file_data.hpp>
#include <file/config/texture_config.hpp>
#include <unordered_map>
#include <vector>
namespace render{
    class file_resources;
    // 管理源图片及其 BC 压缩缓存；这里只处理 CPU 数据，不创建 Vulkan 纹理。
    class image_data final:public file_data{
        friend class file_resources;
    public:
        struct image_entry{
            texture_config config;//压缩选项及持久化信息；config.guid 用于配置文件读写
            // 完整 DDS 文件字节：文件头和 BC 压缩的各级 mip 数据。
            // 初始为空，第一次 get_image(uid) 时才从 library 读取。
            std::vector<uint8_t> dds;
        };
        // 按 UID 获取缓存并按需加载 DDS；返回 map 内对象的只读引用。
        const image_entry& get_image(const std::string& uid);
        // 查看全部索引，不触发 DDS 文件加载。
        const auto& get_entries() const noexcept{return images;}
    private:
        image_data():file_data(VK_NULL_HANDLE){}
        // 从 scan 查询配置路径，只加载配置，不持有路径、不写文件。
        void register_library(const std::string& uid) override;
        // 源图解码、压缩和缓存写入由 scan 的导入接口处理。
        void register_data(const std::string&) override{}
        // 只删除内存记录；DDS、配置及 .guid 由 scan 删除。
        void remove_data(const std::string& uid) override;
        // 退出时只清空内存，保留磁盘缓存供下次启动复用。
        void cleanup() noexcept override{images.clear();}
        // key 是源图片 UID，value 是该图片的缓存记录，直接存储对象而非引用。
        // 通过 images.find(uid) 查找；image_entry 不再重复保存 UID。
        std::unordered_map<std::string,image_entry> images;
    };
}
#endif
