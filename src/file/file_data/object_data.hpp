#ifndef __RENDER_OBJECT_DATA_HPP
#define __RENDER_OBJECT_DATA_HPP

#include <file/file_data/file_data.hpp>
#include <file/config/object_config.hpp>
#include <unordered_map>

namespace render{
    class file_resources;
    // 对象配置缓存；读取配置不会实例化 object 或分配 GPU 资源。
    class object_data final:public file_data{
        friend class file_resources;
    public:
        object_config* get_object(const std::string& uid) noexcept;
        const object_config* get_object(const std::string& uid) const noexcept;
    private:
        struct object_entry{
            object_config config;
        };
        object_data():file_data(VK_NULL_HANDLE){}
        void register_data(const std::string& uid) override;
        void register_library(const std::string&) override{}
        void remove_data(const std::string& uid) override;
        void cleanup() noexcept override;
        std::unordered_map<std::string,object_entry> objects;
    };
}

#endif // __RENDER_OBJECT_DATA_HPP


