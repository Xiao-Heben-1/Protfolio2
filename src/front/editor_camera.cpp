#include <front/editor.hpp>
namespace render::front {
void editor::camera_inspector(camera& cam){
    auto pose=cam.get_config();
    ImGui::Text("is_main: %s",cam.get_is_main() ? "true" : "false");
    if(!cam.get_is_main() && ImGui::Button("Set Main")){cam.set_main();scene_dirty=true;}
        ImGui::TextUnformatted("Camera");ImGui::Separator();
        bool changed=ImGui::DragFloat3("Position",pose.position.data(),.1f);
        changed|=ImGui::DragFloat3("Direction",pose.direction.data(),.01f);
        changed|=ImGui::SliderFloat("Field of View",&pose.fov,10,120);
        changed|=ImGui::DragFloat("Near",&pose.near_plane,.01f,.01f,100);
        changed|=ImGui::DragFloat("Far",&pose.far_plane,1,100,20000);
        changed|=ImGui::DragFloat4("LOD widths",pose.dis.data(),1,1,20000);
        if(changed){cam.get_behavior().get_input().pose=pose;scene_dirty=true;}
        ImGui::Text("Target: %u x %u | Aspect %.3f",pose.width,pose.height,pose.aspect);
        ImGui::Text("Color format: %u | Depth format: %u",pose.g_buffer_format,pose.depth_format);
        ImGui::Spacing();ImGui::Separator();ImGui::TextUnformatted("Camera Navigation (Behavior)");
        auto& navigation=cam.get_behavior().get_navigation();
        scene_dirty|=ImGui::DragFloat("Move speed",&navigation.move_speed,.1f,.1f,1000);
        scene_dirty|=ImGui::DragFloat("Zoom speed",&navigation.zoom_speed,.02f,.01f,100);
        scene_dirty|=ImGui::DragFloat("Look sensitivity",&navigation.look_speed,.00005f,.0001f,.01f,"%.5f");
        scene_dirty|=ImGui::DragFloat("Pan sensitivity",&navigation.pan_speed,.0001f,.0001f,1,"%.4f");
        scene_dirty|=ImGui::DragFloat("Fast multiplier",&navigation.fast_multiplier,.1f,1,20);
        ImGui::TextDisabled("Built in camera navigation");
}
}
