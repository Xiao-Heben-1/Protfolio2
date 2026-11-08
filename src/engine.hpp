#ifndef __RENDER_ENGINE
#define __RENDER_ENGINE

#include <file/file_system.hpp>
#include <core/core.hpp>
#include <memory>
namespace render
{
    class engine{
    public:
        void init(const std::filesystem::path& project);
        void init(workspace_config config);
        void shutdown();
        void init_render(){rd.init(fs->get_config().builtin_shader_root);}
        file_system& get_file_system() noexcept { return *fs; }
        const file_system& get_file_system() const noexcept { return *fs; }
        render_system& get_render_system() noexcept { return rd; }
        const render_system& get_render_system() const noexcept { return rd; }
    private:
        render_system rd;//渲染系统
        std::unique_ptr<file_system> fs;//先释放文件持有的管线，再销毁渲染设备
    };
    extern class engine egn;
} 


#endif
