#ifndef __RENDER_FILE_RESOURCES_HPP
#define __RENDER_FILE_RESOURCES_HPP
#include <file/file_data/file_data_enter.hpp>
#include <file/file_data/render_pass_data.hpp>

namespace render{
    // 仅持有各类 data；文件增删、导入及 library 持久化统一由 scan 调用。
    class file_resources{
        friend class file_scan;
    public:
        explicit file_resources(VkDevice device = VK_NULL_HANDLE):shader(device),spirv(device){}
        spirv_data& get_spirv_data() noexcept{return spirv;}
        const spirv_data& get_spirv_data() const noexcept{return spirv;}
        scene_data& get_scene_data() noexcept{return scene;}
        const scene_data& get_scene_data() const noexcept{return scene;}
        object_data& get_object_data() noexcept{return object;}
        const object_data& get_object_data() const noexcept{return object;}
        image_data& get_image_data() noexcept{return image;}
        const image_data& get_image_data() const noexcept{return image;}
        render_pass_data& get_render_pass_data() noexcept{return render_pass;}
        const render_pass_data& get_render_pass_data() const noexcept{return render_pass;}
        void set_device(VkDevice device) noexcept{spirv.device = device;}
    private:
        render_pass_data render_pass;
        shader_data shader;
        spirv_data spirv;
        scene_data scene;
        object_data object;
        image_data image;
        file_data* get_data(file_type type) noexcept{
            switch(type){
                case file_type::render_pass:
                case file_type::shader_config:
                case file_type::sub_shader: return &render_pass;
                case file_type::shader: return &shader;
                case file_type::spirv: return &spirv;
                case file_type::scene: return &scene;
                case file_type::object: return &object;
                case file_type::image:
                case file_type::texture: return &image;
                default: return nullptr;
            }
        }
    };
}
#endif
