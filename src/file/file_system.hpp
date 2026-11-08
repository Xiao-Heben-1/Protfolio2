#ifndef __RENDER_FILE_SYSTEM_HPP
#define __RENDER_FILE_SYSTEM_HPP

#include <filesystem>
#include <file/config/workspace_config.hpp>
#include <string>
#include <unordered_map>
#include <file/file_info.hpp>
#include <file/file_scan.hpp>
#include <file/file_resources.hpp>

namespace render{
    class file_system final{
        friend class file_scan;
    public:
        explicit file_system(const std::filesystem::path& root_path,VkDevice device = VK_NULL_HANDLE);
        explicit file_system(workspace_config config,VkDevice device = VK_NULL_HANDLE);
        const workspace_config& get_config() const noexcept{return config;}
        ~file_system();
        const file_resources& get_resources() const noexcept { return res; }
        file_resources& get_resources() noexcept { return res; }
        const auto& get_files() const noexcept { return files; }
        const auto& get_geometry_files() const noexcept { return geometry_files; }
        file_scan& get_scan() noexcept{return fs;}
        const file_scan& get_scan() const noexcept{return fs;}
        // 在主线程每帧调用：应用监听线程收集的变化，资源缓存不在后台修改。
        void update();

    private:
        workspace_config config;
        std::unordered_map<std::string,file_info> files;//源资源与派生配置索引；generated 字段区分归属
        std::unordered_map<std::string,file_info> geometry_files;//几何GUID -> 文件头信息，路径相对于library根目录
        file_resources res;
        file_scan fs;//最后析构前先停止监听
    };
}
#endif // __RENDER_FILE_SYSTEM_HPP
