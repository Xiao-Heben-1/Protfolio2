#include <front/editor.hpp>
namespace render::front {
void editor::light_inspector(light& value){
    auto config=value.get_config();bool changed=false;
    if(config.type==light_type::main){
        ImGui::TextUnformatted("Sun / main_light");
        changed|=ImGui::DragFloat3("Solar irradiance (RGB)",config.solar_irradiance.data(),.1f,0,100000,"%.3f W/m2");
        float degrees=glm::degrees(config.angular_radius);
        if(ImGui::DragFloat("Angular radius",&degrees,.01f,.01f,30,"%.3f deg")){config.angular_radius=glm::radians(degrees);changed=true;}
        changed|=ImGui::DragFloat("Sun disk multiplier",&config.disk_intensity,.01f,0,100);
        ImGui::TextWrapped("Local direction follows the light rays; the sky uses the opposite direction toward the Sun.");
    }else{
        changed|=ImGui::ColorEdit3("Color",config.color.data());
        changed|=ImGui::DragFloat("Intensity",&config.emission,.05f,0,1000);
    }
    if(config.type==light_type::point){
        changed|=ImGui::DragFloat3("Position offset",config.position.data(),.02f);
        changed|=ImGui::DragFloat("Linear attenuation",&config.k1,.01f,0,100);
        changed|=ImGui::DragFloat("Quadratic attenuation",&config.k2,.01f,0,100);
    }else changed|=ImGui::DragFloat3("Local direction",config.direction.data(),.01f);
    if(changed){value.apply_config(config);scene_dirty=true;}
}
void editor::lighting_inspector(){
    ImGui::TextUnformatted("Scene Lights");ImGui::Separator();
    auto& sky=egn.get_render_system().get_skybox_data();
    if(ImGui::CollapsingHeader("Procedural Sky",ImGuiTreeNodeFlags_DefaultOpen)){
        scene_dirty|=ImGui::DragFloat("Exposure",&sky.exposure,.01f,.001f,100);
        scene_dirty|=ImGui::ColorEdit3("Ground color",&sky.ground_color.x);
        scene_dirty|=ImGui::ColorEdit3("Ground albedo",&sky.ground_albedo.x);
        scene_dirty|=ImGui::DragFloat("Ground intensity",&sky.ground_intensity,.01f,0,10);
        scene_dirty|=ImGui::DragFloat("Atmosphere density",&sky.atmosphere_density,.01f,0,10);
        scene_dirty|=ImGui::DragFloat("Mie g",&sky.mie_g,.001f,-.99f,.99f);
        if(ImGui::TreeNode("Atmosphere parameters")){
            scene_dirty|=ImGui::DragFloat("Earth radius (m)",&sky.earth_radius,100,1,1e8,"%.0f");
            scene_dirty|=ImGui::DragFloat("Atmosphere height (m)",&sky.atmosphere_height,100,1,1e7,"%.0f");
            scene_dirty|=ImGui::DragFloat("Rayleigh scale (m)",&sky.rayleigh_scale_height,10,1,1e6);
            scene_dirty|=ImGui::DragFloat("Mie scale (m)",&sky.mie_scale_height,10,1,1e6);
            scene_dirty|=ImGui::DragFloat3("Rayleigh (1/m)",&sky.rayleigh_scattering.x,1e-7f,0,.001f,"%.8f");
            scene_dirty|=ImGui::DragFloat3("Mie scattering (1/m)",&sky.mie_scattering.x,1e-7f,0,.001f,"%.8f");
            scene_dirty|=ImGui::DragFloat3("Mie extinction (1/m)",&sky.mie_extinction.x,1e-7f,0,.001f,"%.8f");
            scene_dirty|=ImGui::DragFloat3("Ozone absorption (1/m)",&sky.ozone_absorption.x,1e-7f,0,.001f,"%.8f");
            scene_dirty|=ImGui::DragFloat("Ozone center (m)",&sky.ozone_center_height,100,1,1e6);
            scene_dirty|=ImGui::DragFloat("Ozone half width (m)",&sky.ozone_half_width,100,1,1e6);
            scene_dirty|=ImGui::SliderInt("View samples",&sky.view_sample_count,1,64);
            scene_dirty|=ImGui::SliderInt("Light samples",&sky.light_sample_count,1,32);
            ImGui::TreePop();
        }
    }
    for(size_t i=0;i<objects.size();++i)for(auto* comp:objects[i].runtime->get_components()){
        const auto type=component_type(comp->get_type());
        if(type!=component_type::main_light_type && type!=component_type::point_light_type && type!=component_type::directional_light_type)continue;
        ImGui::PushID(comp);
        if(ImGui::Selectable(objects[i].config.name.c_str())){selected_object=int(i);lighting_selected=false;camera_selected=false;}
        light_inspector(*static_cast<light*>(comp));ImGui::Separator();ImGui::PopID();
    }
}
}
