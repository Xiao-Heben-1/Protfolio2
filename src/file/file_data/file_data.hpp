#ifndef __RENDER_FILE_DATA_HPP
#define __RENDER_FILE_DATA_HPP

#include <cstring>
#include <file/file_info.hpp>
#include <vulkan/vulkan.h>
namespace render{
    class file_scan;
    class file_data{
        friend class file_scan;
    public:
        virtual ~file_data() = default;
    protected:
        VkDevice device;
        file_scan* scan = nullptr;//路径与持久化统一由 scan 管理
        explicit file_data(VkDevice device):device(device){}
        // 由 scan 调用；data 只登记/加载内存数据，路径临时向 scan 查询。
        virtual void register_data(const std::string& uid) = 0;
        virtual void register_library(const std::string& uid){};
        // 只释放该 UID 的内存记录，磁盘资源由 scan 删除。
        virtual void remove_data(const std::string& uid) = 0;
        virtual void cleanup() noexcept = 0;
    };
    
}

#endif
