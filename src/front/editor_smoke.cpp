#include <front/editor.hpp>
#include <core/object/object_loader.hpp>
#include <stdexcept>
namespace render::front {
void editor::component_smoke_input(){
    auto& io=ImGui::GetIO();
    // 隐藏窗口没有系统 hover/focus 通知，测试输入指定主视口。
    io.AddFocusEvent(true);
    io.AddMouseViewportEvent(ImGui::GetMainViewport()->ID);
    if(frame_count==20){object_config config;config.name="component_test";add_object(config);show_global_passes=false;}
    if(frame_count>=21 && frame_count<77){
        const int step=int(frame_count-21);component_smoke_index=step/8;
        const int phase=step%8;
        // 弹窗首次出现后留一帧，让自动尺寸与屏幕边缘定位完成。
        const auto point=phase<2 ? add_component_button : component_menu_entry;
        if(phase==0 || phase==1 || phase==3 || phase==4){io.AddMousePosEvent(point.x,point.y);io.AddMouseButtonEvent(0,phase==0 || phase==3);}
        if(phase==6 && objects.back().runtime->get_components().size()!=size_t(component_smoke_index+2))
            throw std::runtime_error("Add Component click failed at menu entry "+std::to_string(component_smoke_index));
    }
    if(frame_count==80){
        auto config=object_loader::describe(*objects.back().runtime,objects.back().config);
        const auto path=egn.get_file_system().get_config().library_root/"generated/tests/component_test.object";
        egn.get_file_system().get_scan().save_object(config.guid,path,config);
        auto saved=object_config::load(path);
        std::unique_ptr<object> restored(object_loader::load(saved));
        if(restored->get_components().size()!=8 || !saved.camera || !saved.mesh || !saved.material || !saved.behavior || saved.lights.size()!=3)
            throw std::runtime_error("Added components did not survive save/reload");
        restored.reset();objects.pop_back();selected_object=0;camera_selected=false;component_smoke_complete=true;
        log("Add Component: seven real menu clicks and save/reload passed");
    }
}
}
