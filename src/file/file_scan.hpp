#ifndef __RENDER_FILE_SCAN_HPP
#define __RENDER_FILE_SCAN_HPP

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <filesystem>
#include <string>
#include <thread>
#include <unordered_map>
#include <mutex>
#include <vector>
#include <span>

#include <file/file_info.hpp>
#include <file/config/resource/mesh_lod_resource.hpp>
#include <functional>
#include <tools/math/random.hpp>

namespace render{
    class file_system;
    struct texture_config;
    struct scene_config;
    struct object_config;
    struct pass_config;
    struct shader_config;
    struct sub_shader_config;

    /// @brief 文件扫描与监听
    /// 初始扫描文件，并实时监听文件创建、删除、移入、移出、改名和移动
    class file_scan final{
        friend class file_system;
        friend class object_loader;
        friend class geometry_cache_test;

    public:
        // data 临时查询读文件的路径，不自行保存路径或写入磁盘。
        std::filesystem::path get_source_path(const std::string& uid) const;
        std::filesystem::path get_library_path(const std::string& uid) const;
        // 所有源资源创建、删除、配置保存都从 scan 进入。
        std::string create_file(const std::filesystem::path& path,std::span<const uint8_t> bytes);
        void delete_file(const std::string& uid);
        // 读取引擎定义和工作空间配置，物化至 library/generated。
        void generate_system_config();
        void save_project_config(const std::string& guid);
        void set_texture_config(const std::string& uid,texture_config config);
        void save_scene(const std::string& uid);
        void save_scene(const std::string& uid,const std::filesystem::path& path,const scene_config& config);
        void save_pass(const std::filesystem::path& path,const pass_config& config);
        void save_shader(const std::filesystem::path& path,const shader_config& config);
        void save_sub_shader(const std::filesystem::path& path,const sub_shader_config& config);
        void save_object(const std::string& guid);
        void save_object(const std::string& guid,const std::filesystem::path& path,const object_config& config);

    private:
        std::array<mesh_lod_resource,3> load_mesh_resources(const std::filesystem::path& source,bool transparent,
            const std::function<mesh_data()>& build,page_manager& pages);
        void register_geometry(const mesh_lod_resource& resource);
        void restore_geometry(const std::string& guid);
        /// @brief 文件最终注册到的文件系统
        file_system& system;
        /// @brief 当前监听根目录
        std::filesystem::path root_path;
        std::filesystem::path library_root;
        bool in_library(const std::filesystem::path& path) const;
        void register_generated(const std::filesystem::path& path);
        void register_config(const std::filesystem::path& relative,const std::string& guid);
        /// @brief Windows 目录句柄
        HANDLE directory_handle = INVALID_HANDLE_VALUE;
        /// @brief 用于通知监听线程停止
        HANDLE stop_event = nullptr;
        /// @brief 文件监听线程
        std::thread monitor_thread;
        /// @brief 保存重命名/移动事件中的旧路径
        std::filesystem::path rename_old_path;
        /// @brief 项目相对路径到uid的索引
        std::unordered_map<std::string, std::string> path_mp;
        /// @brief 缓存 GUID 到文件信息的索引，path 相对于 library_root，source_path 相对于 source_root
        struct library_entry{
            std::filesystem::path path;
            std::filesystem::path source_path;
            file_type type;
            std::string signature;
        };
        std::unordered_map<std::string,library_entry> library_files;
        bool initialized = false;
        bool shaders_dirty = false;
        struct event{DWORD action; std::filesystem::path path;};
        std::mutex event_mutex;
        std::vector<event> pending_events;
        bool rescan_pending = false;
        void process_events();

        /// @brief 创建文件扫描器
        explicit file_scan(file_system& system);
        /// @brief 销毁文件扫描器
        ~file_scan();

        void init(const std::filesystem::path& path);
        void import_source(const std::string& uid);
        void import_image(const std::string& uid);
        void import_spirv(const std::string& uid);
        void remove_source(const std::string& uid);
        void remove_library(const std::string& uid);

        /// @brief 扫描目录中已有的全部文件并注册到 file_system
        void scan(const std::filesystem::path& path = {});

        /// @brief 开始实时监听目录
        void start_monitor(const std::filesystem::path& root_path);
        
        /// @brief 停止实时监听
        void stop_monitor();

        /// @brief 根据文件后缀获得文件类型
        file_type get_file_type(const std::filesystem::path& path) const noexcept;

        /// @brief Windows 文件监听循环
        void monitor_loop();

        /// @brief 分发 Windows 文件变化事件
        void handle_event(DWORD action, const std::filesystem::path& path);

        /// @brief 添加文件
        /// 读取或创建 .guid，并注册到 file_system
        void add_file(const std::filesystem::path& path);

        /// @brief 删除文件
        /// 删除对应 .guid，并从 file_system 注销
        void remove_file(const std::filesystem::path& path);

        /// @brief 文件在监听目录内部改名或移动
        /// 保留原 uid，并移动对应 .guid
        void move_file(const std::filesystem::path& old_path, const std::filesystem::path& new_path);

        /// @brief 读取文件的 .guid；不存在则创建
        std::string load_guid(const std::filesystem::path& path);

        /// @brief 创建文件对应的 .guid
        std::string create_guid(const std::filesystem::path& path);
    };
}

#endif // __RENDER_FILE_SCAN_HPP

