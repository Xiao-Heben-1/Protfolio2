#include <core/object/object.hpp>
#include <engine.hpp>
#include <core/object/object_manger.hpp>
#include <core/render/render_resources.hpp>
#include <core/render/render_system.hpp>
#include <core/component/component_enter.hpp>
#include <algorithm>
namespace render{
    object::object(){
        auto& manager = egn.get_render_system().get_scene().get_object_manger();
        auto& list = is_transparent ? manager.transparent_objects : manager.opaque_objects;
        new transform(this);
        run_id = manager.gen.generate_uid();
        object_idx = static_cast<uint32_t>(manager.objects.size());
        idx = static_cast<uint32_t>(list.size());
        manager.objects.push_back(this);
        list.push_back(this);
    }

    object::~object(){
        egn.get_render_system().get_prim_table().erase(run_id);
        while(!components.empty()) delete components.back();
        auto& manager = egn.get_render_system().get_scene().get_object_manger();
        auto& list = is_transparent ? manager.transparent_objects : manager.opaque_objects;
        list[idx] = list.back();
        list[idx]->idx = idx;
        list.pop_back();
        manager.objects[object_idx] = manager.objects.back();
        manager.objects[object_idx]->object_idx = object_idx;
        manager.objects.pop_back();
    }

    void object::set_transparent(bool value){
        if(value == is_transparent) return;
        auto& manager = egn.get_render_system().get_scene().get_object_manger();
        auto& source = is_transparent ? manager.transparent_objects : manager.opaque_objects;
        auto& target = value ? manager.transparent_objects : manager.opaque_objects;
        const auto new_idx = static_cast<uint32_t>(target.size());
        target.push_back(this);
        source[idx] = source.back();
        source[idx]->idx = idx;
        source.pop_back();
        idx = new_idx;
        is_transparent = value;
        if(mesh) mesh->set_transparent(value);
    }

    void object::upload(VkCommandBuffer command_buffer,VkPipelineLayout pipeline_layout){
        for(component* comp : components){
            comp->upload(command_buffer,pipeline_layout);
        }
    }

    void object::update(){
        for(component* comp : components){
            if(comp != tran) comp->update();
        }
        if(tran != nullptr) tran->update();//其他组件修改位置后再更新 Morton 编码
    }

    void object::late_update(){
        if(!tran || !mesh || (mesh->get_lods()[0].guid.empty() && mesh->get_data().sub_meshes.empty())){
            egn.get_render_system().get_prim_table().erase(run_id);return;
        }
        prim_instance instance;
        instance.id=run_id;
        instance.bounds=mesh->get_bounds().transformed(tran->get_matrix());
        const auto& lods=mesh->get_lods();
        for(size_t i=0;i<3;++i)instance.lod_guids[i]=lods[i].guid;
        egn.get_render_system().get_prim_table().upsert(instance);
    }

    component* object::create_component(int type){
        switch(static_cast<component_type>(type)){
            case component_type::mesh_type:{
                render_resources& res = egn.get_render_system().get_resources();
                return new ::render::mesh(this,res.physical_device,res.device,res.command_pool,res.graphics_queue);
            }
            case component_type::material_type:
                return new material(this);
            case component_type::transform_type:
                return new transform(this);
            case component_type::behavior_type:
                return new behavior(this);
            case component_type::camera_type: return new camera(this);
            case component_type::main_light_type: return new main_light(this);
            case component_type::point_light_type: return new point_light(this);
            case component_type::directional_light_type: return new directional_light(this);
            default:
                return nullptr;
        }
    }
    
    component* object::add_component(int type){
        return create_component(type);
    }

    void object::remove_component(component* comp) noexcept{
        if(comp == nullptr || comp->owner != this) return;
        delete comp;
    }

}


