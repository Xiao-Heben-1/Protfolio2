#include <core/object/object_loader.hpp>
#include <core/object/object.hpp>
#include <engine.hpp>
#include <file/tools/file_path.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <bit>
#include <memory>
#include <algorithm>
namespace render {
namespace {
constexpr const char* texture_names[]={"base_color","normal","metallic","roughness","ao","emissive","opacity"};
std::array<material_texture*,7> textures(sub_material& sub){return {&sub.base_color_map,&sub.normal_map,&sub.metallic_map,&sub.roughness_map,&sub.ao_map,&sub.emissive_map,&sub.opacity_map};}
}
object* object_loader::load(const object_config& config){
    auto& fs=egn.get_file_system();
    constexpr const char* builtins[]={"cube","sphere","capsule","cylinder","plane","quad"};
    const auto builtin=config.mesh ? std::find(std::begin(builtins),std::end(builtins),config.mesh->builtin_geometry)-std::begin(builtins) : 6;
    std::unique_ptr<object> result(builtin<6 ? create_geometry(geometry_type(builtin)) : config.mesh && !config.mesh->source_guid.empty() ? load(path_to_utf8(fs.get_scan().get_source_path(config.mesh->source_guid))) : new object());
    auto* tran=result->get_transform();tran->position=glm::make_vec3(config.transform.position.data());tran->rotation=glm::make_vec3(config.transform.rotation.data());tran->scale=glm::make_vec3(config.transform.scale.data());
    result->set_transparent(config.is_transparent);
    if(config.mesh && !result->get_mesh())result->add_component(int(component_type::mesh_type));
    if(config.material){
        auto* mat=result->get_material();if(!mat)mat=static_cast<material*>(result->add_component(int(component_type::material_type)));
        if(mat->sub_materials.empty())mat->add_sub_material();
        for(const auto& saved:config.material->overrides){
            while(mat->sub_materials.size()<=saved.slot)mat->add_sub_material();
            auto& sub=*mat->sub_materials[saved.slot];auto& u=sub.uniform_data;
            if(saved.base_color)u.base_color=glm::make_vec4(saved.base_color->data());if(saved.emissive_color)u.emissive_color=glm::make_vec4(saved.emissive_color->data());
            if(saved.metallic)u.parameter.x=*saved.metallic;if(saved.roughness)u.parameter.y=*saved.roughness;if(saved.ao)u.parameter.z=*saved.ao;if(saved.emissive_strength)u.parameter.w=*saved.emissive_strength;
            if(saved.alpha_mode)u.texture_flags_2.z=*saved.alpha_mode;if(saved.alpha_cutoff)u.texture_flags_2.w=std::bit_cast<uint32_t>(*saved.alpha_cutoff);
            if(saved.base_color_alpha)u.alpha_options.x=*saved.base_color_alpha;
            if(saved.visible)sub.visible=*saved.visible;
            if(saved.shader_guid)sub.shader_guid=*saved.shader_guid;
            if(saved.texture_channels)u.texture_channels=glm::make_vec4(saved.texture_channels->data());
            const auto images=textures(sub);
            for(size_t i=0;i<images.size();++i)if(auto found=saved.textures.find(texture_names[i]);found!=saved.textures.end()){
                images[i]->image_uid=found->second;const uint32_t enabled=!found->second.empty();
                if(i<4)u.texture_flags[int(i)]=enabled;else if(i<6)u.texture_flags_2[int(i-4)]=enabled;else u.texture_channels.z=enabled;
            }
        }
    }
    if(config.behavior)result->add_component(int(component_type::behavior_type));
    if(config.camera){
        auto* cam=result->add_component<camera>();cam->apply_config(*config.camera);cam->update();
        if(config.camera->is_main)cam->set_main();
    }
    for(const auto& saved:config.lights){
        const auto type=saved.type==light_type::main ? component_type::main_light_type : saved.type==light_type::point ? component_type::point_light_type : component_type::directional_light_type;
        static_cast<light*>(result->add_component(int(type)))->apply_config(saved);
    }
    return result.release();
}
object_config object_loader::describe(const object& value,object_config config){
    const auto* tran=value.get_transform();
    config.transform.position={tran->position.x,tran->position.y,tran->position.z};config.transform.rotation={tran->rotation.x,tran->rotation.y,tran->rotation.z};config.transform.scale={tran->scale.x,tran->scale.y,tran->scale.z};
    config.is_transparent=value.get_transparent();
    config.camera.reset();if(const auto* cam=value.get_camera())config.camera=cam->get_config();
    config.lights.clear();config.behavior=false;
    for(const auto* comp:value.get_components()){
        const auto type=component_type(comp->get_type());
        if(type==component_type::behavior_type)config.behavior=true;
        if(type==component_type::main_light_type || type==component_type::point_light_type || type==component_type::directional_light_type)
            config.lights.push_back(static_cast<const light*>(comp)->get_config());
    }
    if(const auto* mesh=value.get_mesh()){
        if(!config.mesh)config.mesh.emplace();config.mesh->bounds=mesh->get_bounds();
        config.mesh->source_guid=mesh->get_lods()[0].source_guid;config.mesh->builtin_geometry=mesh->builtin_geometry;
        if(mesh->get_lods()[0].guid.empty() && mesh->get_data().sub_meshes.empty())config.mesh->bounds.reset();
        for(size_t i=0;i<3;++i)config.mesh->lod_guids[i]=mesh->get_lods()[i].guid;
    }else config.mesh.reset();
    if(const auto* mat=value.get_material()){
        if(!config.material)config.material.emplace();config.material->overrides.clear();
        for(size_t i=0;i<mat->get_sub_materials().size();++i){
            auto& sub=*mat->get_sub_material(uint32_t(i));const auto& u=sub.uniform_data;material_override_config saved;saved.slot=uint32_t(i);
            saved.base_color={u.base_color.x,u.base_color.y,u.base_color.z,u.base_color.w};saved.emissive_color={u.emissive_color.x,u.emissive_color.y,u.emissive_color.z,u.emissive_color.w};
            saved.metallic=u.parameter.x;saved.roughness=u.parameter.y;saved.ao=u.parameter.z;saved.emissive_strength=u.parameter.w;
            saved.alpha_mode=u.texture_flags_2.z;saved.alpha_cutoff=std::bit_cast<float>(u.texture_flags_2.w);saved.shader_guid=sub.shader_guid;
            saved.base_color_alpha=u.alpha_options.x!=0;saved.visible=sub.visible;
            saved.texture_channels={u.texture_channels.x,u.texture_channels.y,u.texture_channels.z,u.texture_channels.w};
            const auto images=textures(sub);for(size_t t=0;t<images.size();++t)saved.textures[texture_names[t]]=images[t]->image_uid;
            config.material->overrides.push_back(std::move(saved));
        }
    }else config.material.reset();
    return config;
}
}
