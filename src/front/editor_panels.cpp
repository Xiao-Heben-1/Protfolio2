#include <front/editor.hpp>
#include <imgui_internal.h>
#include <glm/gtc/type_ptr.hpp>
#include <algorithm>
#include <array>
#include <cstring>
#include <limits>
#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>
namespace render::front {
namespace {
bool text_field(const char* label,std::string& value){std::array<char,512> buffer{};std::copy_n(value.c_str(),std::min(value.size(),buffer.size()-1),buffer.data());if(!ImGui::InputText(label,buffer.data(),buffer.size()))return false;value=buffer.data();return true;}
const char* type_name(file_type type){
    switch(type){case file_type::model:return "MODEL";case file_type::scene:return "SCENE";case file_type::object:return "OBJECT";case file_type::shader:return "SOURCE";case file_type::shader_config:return "SHADER";case file_type::sub_shader:return "STAGE";case file_type::render_pass:return "PASS";case file_type::image:return "IMAGE";default:return "FILE";}
}
}
void editor::default_layout(ImGuiID dock){
    if(ImGui::DockBuilderGetNode(dock))return;
    ImGui::DockBuilderAddNode(dock,ImGuiDockNodeFlags_DockSpace);
    ImGui::DockBuilderSetNodeSize(dock,ImGui::GetMainViewport()->WorkSize);
    auto center=dock;
    const auto left=ImGui::DockBuilderSplitNode(center,ImGuiDir_Left,.18f,nullptr,&center);
    const auto right=ImGui::DockBuilderSplitNode(center,ImGuiDir_Right,.38f,nullptr,&center);
    const auto bottom=ImGui::DockBuilderSplitNode(center,ImGuiDir_Down,.28f,nullptr,&center);
    ImGui::DockBuilderDockWindow("Hierarchy",left);ImGui::DockBuilderDockWindow("Inspector",right);
    ImGui::DockBuilderDockWindow("Scene",center);ImGui::DockBuilderDockWindow("Project",bottom);ImGui::DockBuilderDockWindow("Console",bottom);
    ImGui::DockBuilderFinish(dock);
}
void editor::draw(){
    menu();
    const auto* view=ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(view->WorkPos);ImGui::SetNextWindowSize(view->WorkSize);ImGui::SetNextWindowViewport(view->ID);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,ImVec2(0,0));ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding,0);
    ImGui::Begin("EditorDockHost",nullptr,ImGuiWindowFlags_NoDocking|ImGuiWindowFlags_NoTitleBar|ImGuiWindowFlags_NoResize|ImGuiWindowFlags_NoMove|ImGuiWindowFlags_NoBringToFrontOnFocus|ImGuiWindowFlags_NoNavFocus);
    dock_id=ImGui::GetID("RenderEditorDock");default_layout(dock_id);ImGui::DockSpace(dock_id);
    ImGui::End();ImGui::PopStyleVar(2);
    hierarchy();viewport();inspector();project();console();
    if(show_global_passes)global_passes();
}
void editor::menu(){
    if(!ImGui::BeginMainMenuBar())return;
    if(ImGui::BeginMenu("File")){
        if(ImGui::MenuItem("New Scene"))new_scene();
        if(ImGui::BeginMenu("Open Scene")){
            for(const auto& [guid,file]:egn.get_file_system().get_files())if(file.type==file_type::scene && ImGui::MenuItem(file.path.generic_string().c_str())){open_scene(egn.get_file_system().get_scan().get_source_path(guid));break;}
            ImGui::EndMenu();
        }
        if(ImGui::MenuItem("Save Scene","Ctrl+S"))save_scene();
        ImGui::Separator();if(ImGui::MenuItem("Exit"))glfwSetWindowShouldClose(window,1);
        ImGui::EndMenu();
    }
    if(ImGui::MenuItem("load_model"))choose_model();
    if(ImGui::BeginMenu("GameObject")){
        if(ImGui::BeginMenu("Add Object")){create_object_menu();ImGui::EndMenu();}
        if(ImGui::MenuItem("Delete Selected",nullptr,false,selected_object>=0)){objects.erase(objects.begin()+selected_object);selected_object=-1;camera_selected=true;scene_dirty=true;}
        ImGui::EndMenu();
    }
    if(ImGui::BeginMenu("Component",selected_object>=0)){
        component_menu(objects[size_t(selected_object)]);ImGui::EndMenu();
    }
    if(ImGui::BeginMenu("Rendering")){
        if(ImGui::MenuItem("Global Pass Settings..."))show_global_passes=true;
        ImGui::EndMenu();
    }
    if(ImGui::BeginMenu("Window")){
        if(ImGui::MenuItem("Reset Layout"))ImGui::DockBuilderRemoveNode(dock_id);
        ImGui::MenuItem("Scene Grid",nullptr,&show_grid);ImGui::EndMenu();
    }
    ImGui::Separator();ImGui::TextDisabled("%s%s",scene_document.name.c_str(),scene_dirty ? " *" : "");
    ImGui::EndMainMenuBar();
    if(ImGui::GetIO().KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_S))save_scene();
}
void editor::hierarchy(){
    ImGui::Begin("Hierarchy");
    ImGui::TextDisabled("SCENE");ImGui::Separator();
    if(ImGui::Selectable("Scene Lighting",lighting_selected)){lighting_selected=true;camera_selected=false;selected_object=-1;selected_asset.clear();}
    if(ImGui::Selectable("Active Camera",camera_selected)){
        lighting_selected=false;camera_selected=true;selected_object=-1;selected_asset.clear();
        const auto* active=&egn.get_render_system().get_scene().get_main_camera();
        for(size_t i=0;i<objects.size();++i)if(objects[i].runtime->get_camera()==active){selected_object=int(i);camera_selected=false;break;}
    }
    for(size_t i=0;i<objects.size();++i){
        ImGui::PushID(int(i));
        if(ImGui::Selectable(objects[i].config.name.c_str(),selected_object==int(i))){selected_object=int(i);camera_selected=false;lighting_selected=false;selected_asset.clear();}
        if(ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(0))focus_selected();
        ImGui::PopID();
    }
    ImGui::Spacing();if(ImGui::Button("+ Object",{-1,0}))ImGui::OpenPopup("add object");
    if(ImGui::BeginPopup("add object")){create_object_menu();ImGui::EndPopup();}
    ImGui::End();
}
void editor::viewport(){
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,ImVec2(0,0));ImGui::Begin("Scene");
    if(ImGui::Button("Frame Selected"))focus_selected();ImGui::SameLine();ImGui::Checkbox("Grid",&show_grid);
    ImGui::SameLine();ImGui::TextDisabled("Perspective  |  %.0f FPS",ImGui::GetIO().Framerate);
    auto size=ImGui::GetContentRegionAvail();size.x=std::max(32.f,size.x);size.y=std::max(32.f,size.y);
    requested_extent={uint32_t(size.x),uint32_t(size.y)};
    const auto origin=ImGui::GetCursorScreenPos();
    ImGui::Image((ImTextureID)scene_texture,size);
    const bool hovered=ImGui::IsItemHovered();
    auto& cam=egn.get_render_system().get_scene().get_main_camera();
    auto& input=cam.get_behavior().get_input();const auto& io=ImGui::GetIO();
    if(hovered){
        input.delta_time=io.DeltaTime;input.zoom=io.MouseWheel;
        if(ImGui::IsMouseDown(1)){
            input.look={io.MouseDelta.x,io.MouseDelta.y};input.fast=io.KeyShift;
            input.move={float(ImGui::IsKeyDown(ImGuiKey_D))-float(ImGui::IsKeyDown(ImGuiKey_A)),float(ImGui::IsKeyDown(ImGuiKey_E))-float(ImGui::IsKeyDown(ImGuiKey_Q)),float(ImGui::IsKeyDown(ImGuiKey_W))-float(ImGui::IsKeyDown(ImGuiKey_S))};
        }
        if(ImGui::IsMouseDown(2))input.pan={io.MouseDelta.x,io.MouseDelta.y};
        if(ImGui::IsKeyPressed(ImGuiKey_F))focus_selected();
        if(io.MouseWheel!=0 || glm::dot(input.move,input.move)>0 || ImGui::IsMouseDragging(1) || ImGui::IsMouseDragging(2))scene_dirty=true;
        if(ImGui::IsMouseClicked(0)){
            const glm::vec2 ndc={(io.MousePos.x-origin.x)/size.x*2-1,(io.MousePos.y-origin.y)/size.y*2-1};
            const auto inverse=glm::inverse(cam.get_vp());auto near_point=inverse*glm::vec4(ndc,0,1),far_point=inverse*glm::vec4(ndc,1,1);
            const glm::vec3 start=glm::vec3(near_point)/near_point.w,ray=glm::normalize(glm::vec3(far_point)/far_point.w-start);
            float nearest=std::numeric_limits<float>::max();int hit=-1;
            for(size_t i=0;i<objects.size();++i)if(const auto* mesh=objects[i].runtime->get_mesh()){
                if(mesh->get_lods()[0].guid.empty() && mesh->get_data().sub_meshes.empty())continue;
                const auto bounds=mesh->get_bounds().transformed(objects[i].runtime->get_transform()->get_matrix());
                const auto a=(bounds.get_min()-start)/ray,b=(bounds.get_max()-start)/ray;
                const auto lo=glm::min(a,b),hi=glm::max(a,b);
                const float enter=std::max({lo.x,lo.y,lo.z,0.f}),leave=std::min({hi.x,hi.y,hi.z});
                if(leave>=enter && enter<nearest){nearest=enter;hit=int(i);}
            }
            if(hit>=0){lighting_selected=false;selected_object=hit;camera_selected=false;selected_asset.clear();}
        }
    }
    auto* draw=ImGui::GetWindowDrawList();draw->PushClipRect(origin,{origin.x+size.x,origin.y+size.y},true);
    const auto line=[&](glm::vec3 a,glm::vec3 b,ImU32 color,float width=1){
        const auto va=cam.get_vp()*glm::vec4(a,1),vb=cam.get_vp()*glm::vec4(b,1);
        if(va.w<=0 || vb.w<=0)return;
        const auto pa=glm::vec3(va)/va.w,pb=glm::vec3(vb)/vb.w;
        draw->AddLine({origin.x+(pa.x*.5f+.5f)*size.x,origin.y+(pa.y*.5f+.5f)*size.y},{origin.x+(pb.x*.5f+.5f)*size.x,origin.y+(pb.y*.5f+.5f)*size.y},color,width);
    };
    if(show_grid)for(int i=-12;i<=12;++i){line({float(i),.01f,-12},{float(i),.01f,12},i==0 ? IM_COL32(84,150,210,180) : IM_COL32(120,135,155,42));line({-12,.01f,float(i)},{12,.01f,float(i)},i==0 ? IM_COL32(205,91,91,180) : IM_COL32(120,135,155,42));}
    if(selected_object>=0)if(const auto* mesh=objects[size_t(selected_object)].runtime->get_mesh();mesh && (!mesh->get_lods()[0].guid.empty() || !mesh->get_data().sub_meshes.empty())){
        const auto bounds=mesh->get_bounds().transformed(objects[size_t(selected_object)].runtime->get_transform()->get_matrix());
        std::array<glm::vec3,8> corners;
        for(int i=0;i<8;++i)corners[i]={i&1 ? bounds.get_max().x : bounds.get_min().x,i&2 ? bounds.get_max().y : bounds.get_min().y,i&4 ? bounds.get_max().z : bounds.get_min().z};
        for(int i=0;i<8;++i)for(int bit:{1,2,4})if(!(i&bit))line(corners[i],corners[i|bit],IM_COL32(242,171,67,255),1.5f);
    }
    draw->AddText({origin.x+14,origin.y+14},IM_COL32(190,205,220,255),"SCENE VIEW");
    draw->AddText({origin.x+14,origin.y+size.y-26},IM_COL32(155,175,195,255),"RMB + WASD/QE  |  MMB pan  |  Wheel zoom  |  F focus");
    draw->PopClipRect();ImGui::End();ImGui::PopStyleVar();
}
void editor::inspector(){
    ImGui::Begin("Inspector");
    if(!selected_asset.empty()){edit_asset();ImGui::End();return;}
    if(lighting_selected){lighting_inspector();ImGui::End();return;}
    if(camera_selected){
        camera_inspector(egn.get_render_system().get_scene().get_main_camera());
    }else if(selected_object>=0){
        auto& item=objects[size_t(selected_object)];scene_dirty|=text_field("Name",item.config.name);
        ImGui::TextWrapped("GUID: %s",item.config.guid.c_str());ImGui::Separator();
        auto* tran=item.runtime->get_transform();
        if(ImGui::CollapsingHeader("Transform",ImGuiTreeNodeFlags_DefaultOpen)){
            scene_dirty|=ImGui::DragFloat3("Position",glm::value_ptr(tran->position),.05f);
            auto degrees=glm::degrees(tran->rotation);
            if(ImGui::DragFloat3("Rotation",glm::value_ptr(degrees),.5f)){tran->rotation=glm::radians(degrees);scene_dirty=true;}
            scene_dirty|=ImGui::DragFloat3("Scale",glm::value_ptr(tran->scale),.02f);
        }
        component_inspector(item);
    }else ImGui::TextDisabled("Select an object or an asset");
    ImGui::End();
}
void editor::project(){
    ImGui::Begin("Project");ImGui::SetNextItemWidth(260);ImGui::InputTextWithHint("##search","Search assets...",project_search,sizeof(project_search));
    ImGui::SameLine();if(ImGui::Button("load_model"))choose_model();
    ImGui::SameLine();ImGui::Checkbox("Library",&library_view);
    ImGui::TextWrapped("src: %s",egn.get_file_system().get_config().source_root.string().c_str());
    std::vector<const file_info*> files;
    for(const auto& [guid,file]:egn.get_file_system().get_files())if(file.generated==library_view && (project_search[0]==0 || file.path.generic_string().find(project_search)!=std::string::npos))files.push_back(&file);
    std::sort(files.begin(),files.end(),[](auto* a,auto* b){return a->path<b->path;});
    std::string open;file_type type=file_type::unknown;
    if(ImGui::BeginTable("assets",3,ImGuiTableFlags_RowBg|ImGuiTableFlags_ScrollY|ImGuiTableFlags_BordersInnerV)){
        ImGui::TableSetupColumn("Name");ImGui::TableSetupColumn("Type",ImGuiTableColumnFlags_WidthFixed,80);ImGui::TableSetupColumn("Path");ImGui::TableHeadersRow();
        for(const auto* file:files){ImGui::PushID(file->guid.c_str());ImGui::TableNextRow();ImGui::TableNextColumn();
            if(ImGui::Selectable(file->name.c_str(),selected_asset==file->guid,ImGuiSelectableFlags_SpanAllColumns)){selected_asset=file->guid;lighting_selected=false;camera_selected=false;selected_object=-1;}
            if(ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(0)){open=file->guid;type=file->type;}
            ImGui::TableNextColumn();ImGui::TextDisabled("%s",type_name(file->type));ImGui::TableNextColumn();ImGui::TextDisabled("%s",file->path.generic_string().c_str());ImGui::PopID();
        }
        ImGui::EndTable();
    }
    if(!open.empty())try{if(type==file_type::model)add_model(open);else if(type==file_type::scene)open_scene(egn.get_file_system().get_scan().get_source_path(open));else if(type==file_type::render_pass)select_pass(open);else if(type==file_type::object)add_object_asset(open);}catch(const std::exception& e){log(e.what());}
    ImGui::End();
}
void editor::console(){
    ImGui::Begin("Console");if(ImGui::Button("Clear"))messages.clear();ImGui::Separator();
    for(const auto& message:messages)ImGui::TextWrapped("%s",message.c_str());ImGui::End();
}
}
