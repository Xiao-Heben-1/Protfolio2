#include <front/editor.hpp>
#include <core/object/object_loader.hpp>
#include <file/tools/file_path.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <bit>
namespace render::front {
namespace {
bool resource_field(const char* label,std::string& guid,file_type type){
    ImGui::TextUnformatted(label);ImGui::SetNextItemWidth(-1);
    const auto& files=egn.get_file_system().get_files();auto found=files.find(guid);bool changed=false;
    ImGui::PushID(label);
    if(ImGui::BeginCombo("##resource",found==files.end() ? "null" : found->second.name.c_str())){
        if(ImGui::Selectable("null",guid.empty())){guid.clear();changed=true;}
        for(const auto& [key,file]:files)if(file.type==type){ImGui::PushID(key.c_str());if(ImGui::Selectable(file.name.c_str(),key==guid)){guid=key;changed=true;}ImGui::PopID();}
        ImGui::EndCombo();
    }
    ImGui::PopID();return changed;
}
}
void editor::add_object_asset(const std::string& guid){
    auto config=*egn.get_file_system().get_resources().get_object_data().get_object(guid);
    config.guid=object_config().guid;add_object(std::move(config));objects.back().runtime->update();focus_selected();
}
void editor::create_object_menu(){
    if(ImGui::MenuItem("Empty Object")){object_config config;config.name="Object";add_object(config);}
    if(ImGui::BeginMenu("Geometry")){
        for(const auto* name:{"Cube","Sphere","Capsule","Cylinder","Plane","Quad"})
            if(ImGui::MenuItem(name))try{add_geometry(name);}catch(const std::exception& e){log(e.what());}
        ImGui::EndMenu();
    }
    if(ImGui::MenuItem("Camera")){object_config config;config.name="Camera";config.camera.emplace();add_object(config);}
    if(ImGui::BeginMenu("Light")){
        for(const auto kind:{light_type::main,light_type::point,light_type::directional}){
            const char* name=kind==light_type::main ? "Main Light" : kind==light_type::point ? "Point Light" : "Directional Light";
            if(ImGui::MenuItem(name)){object_config config;config.name=name;config.lights.emplace_back();config.lights.back().type=kind;add_object(config);}
        }ImGui::EndMenu();
    }
    if(!ImGui::BeginMenu("From Asset"))return;
    std::vector<file_info> assets;for(const auto& [guid,file]:egn.get_file_system().get_files())if(file.type==file_type::model || file.type==file_type::object)assets.push_back(file);
    for(const auto& file:assets){ImGui::PushID(file.guid.c_str());
        if(ImGui::MenuItem(file.name.c_str()))try{if(file.type==file_type::model)add_model(file.guid);else add_object_asset(file.guid);}catch(const std::exception& e){log(e.what());}
        ImGui::PopID();
    }
    ImGui::EndMenu();
}
void editor::add_component(scene_item& item,component_type type){
    auto* value=item.runtime->add_component(int(type));
    if(type==component_type::material_type)static_cast<material*>(value)->add_sub_material();
    item.config=object_loader::describe(*item.runtime,item.config);
    scene_dirty=true;log("Component added to "+item.config.name+" ("+std::to_string(item.runtime->get_components().size())+")");
}
void editor::component_menu(scene_item& item){
    const component_type types[]={component_type::mesh_type,component_type::material_type,component_type::behavior_type,component_type::camera_type,
        component_type::main_light_type,component_type::point_light_type,component_type::directional_light_type};
    const char* names[]={"Mesh","Material","Behavior","Camera","Main Light","Point Light","Directional Light"};
    ImGui::MenuItem("Transform (required)",nullptr,true,false);
    for(int i=0;i<7;++i){
        bool existing=false;
        for(const auto* comp:item.runtime->get_components())if(comp->get_type()==uint32_t(types[i]))existing=true;
        if(ImGui::MenuItem(names[i],nullptr,existing,!existing)){
            try{add_component(item,types[i]);}catch(const std::exception& error){log(error.what());}
        }
        if(component_smoke && component_smoke_index==i)
            component_menu_entry={(ImGui::GetItemRectMin().x+ImGui::GetItemRectMax().x)*.5f,(ImGui::GetItemRectMin().y+ImGui::GetItemRectMax().y)*.5f};
    }
}
void editor::component_inspector(scene_item& item){
    auto& obj=*item.runtime;
    if(ImGui::Button("Add Component",{-1,0}))ImGui::OpenPopup("components");
    add_component_button={(ImGui::GetItemRectMin().x+ImGui::GetItemRectMax().x)*.5f,(ImGui::GetItemRectMin().y+ImGui::GetItemRectMax().y)*.5f};
    if(ImGui::BeginPopup("components")){component_menu(item);ImGui::EndPopup();}
    ImGui::TextDisabled("Components: %zu",obj.get_components().size());
    if(auto* cam=obj.get_camera();cam && ImGui::CollapsingHeader("Camera",ImGuiTreeNodeFlags_DefaultOpen)){
        camera_inspector(*cam);
        if(ImGui::Button("Remove Camera")){obj.remove_component(cam);scene_dirty=true;}
    }
    component* removed_light=nullptr;
    for(auto* comp:obj.get_components()){
        const auto type=component_type(comp->get_type());
        if(type!=component_type::main_light_type && type!=component_type::point_light_type && type!=component_type::directional_light_type)continue;
        ImGui::PushID(comp);
        if(ImGui::CollapsingHeader(type==component_type::main_light_type ? "Main Light" : type==component_type::point_light_type ? "Point Light" : "Directional Light",ImGuiTreeNodeFlags_DefaultOpen)){
            light_inspector(*static_cast<light*>(comp));if(ImGui::Button("Remove Light"))removed_light=comp;
        }ImGui::PopID();
    }
    if(removed_light){obj.remove_component(removed_light);scene_dirty=true;}
    if(auto* mesh=obj.get_mesh();mesh && ImGui::CollapsingHeader("Mesh",ImGuiTreeNodeFlags_DefaultOpen)){
        std::string source=item.config.mesh ? item.config.mesh->source_guid : "";
        if(resource_field("Model Source",source,file_type::model)){
            if(!source.empty())try{
                object_loader::load(path_to_utf8(egn.get_file_system().get_scan().get_source_path(source)),&obj);
                item.config.mesh.emplace();item.config.mesh->source_guid=source;scene_dirty=true;
            }catch(const std::exception& e){log(e.what());}
            else{obj.remove_component(mesh);obj.add_component(int(component_type::mesh_type));item.config.mesh.emplace();scene_dirty=true;}
            mesh=obj.get_mesh();
        }
        ImGui::Text("Current LOD: %d",mesh->get_cur_lod());
        const auto& b=mesh->get_bounds();
        if(mesh->get_lods()[0].guid.empty() && mesh->get_data().sub_meshes.empty())ImGui::TextUnformatted("AABB: null");
        else{ImGui::Text("AABB min: %.3f, %.3f, %.3f",b.get_min().x,b.get_min().y,b.get_min().z);
            ImGui::Text("AABB max: %.3f, %.3f, %.3f",b.get_max().x,b.get_max().y,b.get_max().z);}
        for(size_t i=0;i<3;++i){const auto& lod=mesh->get_lods()[i];ImGui::PushID(int(i));
            if(ImGui::TreeNode("LOD","LOD %zu",i)){
                ImGui::Text("Vertices: %llu | Triangles: %llu",(unsigned long long)(lod.vertex_bytes/sizeof(vertex)),(unsigned long long)(lod.index_bytes/12));
                ImGui::TextWrapped("GUID: %s",lod.guid.empty() ? "null" : lod.guid.c_str());ImGui::TextWrapped("File: %s",path_to_utf8(lod.path).c_str());
                ImGui::Text("Pages: %zu | Submeshes: %zu",lod.pages.size(),lod.sub_meshes.size());
                for(const auto& sub:lod.sub_meshes)ImGui::Text("material %u | indices %u | first %u | offset %d",sub.mat_idx,sub.index_count,sub.front_idx,sub.vertex_off);
                ImGui::TreePop();
            }ImGui::PopID();
        }
        bool transparent=obj.get_transparent();if(ImGui::Checkbox("Force Transparent Stage",&transparent)){obj.set_transparent(transparent);scene_dirty=true;}
        if(ImGui::Button("Remove Mesh")){obj.remove_component(mesh);item.config.mesh.reset();scene_dirty=true;}
    }
    if(auto* mat=obj.get_material();mat && ImGui::CollapsingHeader("Material",ImGuiTreeNodeFlags_DefaultOpen)){
        for(size_t i=0;i<mat->get_sub_materials().size();++i){
            auto& sub=*mat->get_sub_material(uint32_t(i));auto& u=sub.uniform_data;ImGui::PushID(int(i));
            if(ImGui::TreeNodeEx("slot",ImGuiTreeNodeFlags_DefaultOpen,"Slot %zu",i)){
                ImGui::TextWrapped("%s",sub.name.empty() ? "Material" : sub.name.c_str());
                scene_dirty|=ImGui::Checkbox("Visible",&sub.visible);
                scene_dirty|=ImGui::ColorEdit4("Base RGBA",glm::value_ptr(u.base_color));
                scene_dirty|=ImGui::ColorEdit3("Emission",glm::value_ptr(u.emissive_color));
                scene_dirty|=ImGui::DragFloat("Emission strength",&u.parameter.w,.02f,0,100);
                scene_dirty|=ImGui::SliderFloat("Metallic",&u.parameter.x,0,1);scene_dirty|=ImGui::SliderFloat("Roughness",&u.parameter.y,0,1);scene_dirty|=ImGui::SliderFloat("AO",&u.parameter.z,0,1);
                const char* modes[]={"Auto","Opaque","Mask","Blend"};int alpha=int(u.texture_flags_2.z);
                if(ImGui::Combo("Alpha mode",&alpha,modes,4)){u.texture_flags_2.z=uint32_t(alpha);scene_dirty=true;}
                bool base_alpha=u.alpha_options.x!=0;
                if(ImGui::Checkbox("Base texture alpha is opacity",&base_alpha)){u.alpha_options.x=base_alpha;scene_dirty=true;}
                if(ImGui::IsItemHovered())ImGui::SetTooltip("Disable for textures with packed game masks. Use Mask or Blend for texture transparency.");
                float cutoff=std::bit_cast<float>(u.texture_flags_2.w);if(ImGui::SliderFloat("Alpha cutoff",&cutoff,0,1)){u.texture_flags_2.w=std::bit_cast<uint32_t>(cutoff);scene_dirty=true;}
                const char* names[]={"Base color map","Normal map","Metallic map","Roughness map","AO map","Emissive map","Opacity map"};
                material_texture* maps[]={&sub.base_color_map,&sub.normal_map,&sub.metallic_map,&sub.roughness_map,&sub.ao_map,&sub.emissive_map,&sub.opacity_map};
                if(ImGui::TreeNode("Textures")){
                    for(int t=0;t<7;++t){if(resource_field(names[t],maps[t]->image_uid,file_type::image)){
                        uint32_t enabled=!maps[t]->image_uid.empty();if(t<4)u.texture_flags[t]=enabled;else if(t<6)u.texture_flags_2[t-4]=enabled;else u.texture_channels.z=enabled;scene_dirty=true;
                    }ImGui::TextDisabled("%u x %u",maps[t]->extent.width,maps[t]->extent.height);}
                    const char* channels[]={"R","G","B","A"};int m=int(u.texture_channels.x),r=int(u.texture_channels.y),o=int(u.texture_channels.w);
                    if(ImGui::Combo("Metallic channel",&m,channels,4)){u.texture_channels.x=m;scene_dirty=true;}
                    if(ImGui::Combo("Roughness channel",&r,channels,4)){u.texture_channels.y=r;scene_dirty=true;}
                    if(ImGui::Combo("Opacity channel",&o,channels,4)){u.texture_channels.w=o;scene_dirty=true;}
                    ImGui::TreePop();
                }
                ImGui::TextWrapped("Shader: %s",sub.shader_guid.empty() ? "null (uses pass shader)" : sub.shader_guid.c_str());
                ImGui::TreePop();
            }ImGui::PopID();
        }
        if(ImGui::Button("+ Material Slot")){mat->add_sub_material();scene_dirty=true;}
        if(ImGui::Button("Remove Material")){obj.remove_component(mat);item.config.material.reset();scene_dirty=true;}
    }
    if(item.config.behavior && ImGui::CollapsingHeader("Behavior")){
        ImGui::TextUnformatted("Script: null");ImGui::TextWrapped("Base behavior attached. No custom script configured.");
        if(ImGui::Button("Remove Behavior")){for(auto* c:obj.get_components())if(c->get_type()==uint32_t(component_type::behavior_type)){obj.remove_component(c);break;}item.config.behavior=false;scene_dirty=true;}
    }
    if(ImGui::Button("Save as Object Asset",{-1,0})){
        item.config=object_loader::describe(obj,item.config);
        egn.get_file_system().get_scan().save_object(item.config.guid,egn.get_file_system().get_config().library_root/"generated/objects"/(item.config.guid+".object"),item.config);
        egn.get_file_system().get_scan().save_project_config(item.config.guid);
        log("Saved Object: "+item.config.name);
    }
}
}
