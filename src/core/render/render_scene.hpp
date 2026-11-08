#ifndef __RENDER_RENDER_SCENE_HPP
#define __RENDER_RENDER_SCENE_HPP
#include <core/component/component_enter.hpp>
#include <core/object/object_enter.hpp>
namespace render{
    class render_scene{
        friend struct camera;
    public:
        render_scene(){main_camera.scene=this;main_camera.is_main=true;}
        ~render_scene(){main_camera.scene=nullptr;}
        camera& get_main_camera() noexcept { return *active_camera; }
        const camera& get_main_camera() const noexcept { return *active_camera; }
        light_manger& get_light_manger() noexcept { return global_light_manger; }
        const light_manger& get_light_manger() const noexcept { return global_light_manger; }
        object_manger& get_object_manger() noexcept { return global_object_manger; }
        const object_manger& get_object_manger() const noexcept { return global_object_manger; }
        void update(){
            if(active_camera==&main_camera)main_camera.update();
            for(object* obj : global_object_manger.objects) obj->update();
            for(object* obj : global_object_manger.objects) obj->late_update();
        }
    private:
        camera main_camera;//主相机
        camera* active_camera=&main_camera;
        void select_camera(camera& value){
            value.extent=active_camera->extent;value.aspect=active_camera->aspect;
            value.g_buffer_format=active_camera->g_buffer_format;value.depth_format=active_camera->depth_format;
            active_camera->is_main=false;active_camera=&value;value.is_main=true;
            value.update_projection();value.update_view();
        }
        light_manger global_light_manger;//全局光照管理
        object_manger global_object_manger;///全局对象管理器
    };
}

#endif
