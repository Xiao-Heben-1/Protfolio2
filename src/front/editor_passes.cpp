#include <front/editor.hpp>
#include <algorithm>
#include <array>
#include <sstream>
namespace render::front {
namespace {
bool text_field(const char* label,std::string& value){std::array<char,1024> buffer{};std::copy_n(value.c_str(),std::min(value.size(),buffer.size()-1),buffer.data());if(!ImGui::InputText(label,buffer.data(),buffer.size()))return false;value=buffer.data();return true;}
bool global_stage(pass_stage stage){return stage==pass_stage::pre_processing || stage==pass_stage::post_processing || stage==pass_stage::skybox;}
bool asset_combo(const char* label,std::string& guid,file_type type){
    const auto& files=egn.get_file_system().get_files();const auto current=files.find(guid);
    const std::string preview=current==files.end() ? "null" : current->second.name;bool changed=false;
    if(ImGui::BeginCombo(label,preview.c_str())){
        if(ImGui::Selectable("null",guid.empty())){guid.clear();changed=true;}
        for(const auto& [key,file]:files)if(file.type==type){
            ImGui::PushID(key.c_str());if(ImGui::Selectable(file.name.c_str(),key==guid)){guid=key;changed=true;}ImGui::PopID();
        }
        ImGui::EndCombo();
    }
    return changed;
}
}
void editor::select_pass(const std::string& guid){
    const auto* value=egn.get_file_system().get_resources().get_render_pass_data().get_pass(guid);
    if(value){pass_draft=*value;selected_pass=guid;show_global_passes=true;}
    else log("Pass configuration is missing.");
}
void editor::global_passes(){
    ImGui::SetNextWindowSize({1050,700},ImGuiCond_FirstUseEver);
    ImGui::Begin("Global Pass Settings",&show_global_passes);
    auto& fs=egn.get_file_system();auto& data=fs.get_resources().get_render_pass_data();
    ImGui::TextDisabled("RENDER PIPELINE / SHADER / SUB SHADER");
    ImGui::TextWrapped("All stages follow shader/core/pass. Pre/post stages are editable here; opaque and transparency stages show their bindings.");
    ImGui::Separator();
    const bool add_pre=ImGui::Button("+ Pre-processing");ImGui::SameLine();
    const bool add_post=ImGui::Button("+ Post-processing");
    if(add_pre || add_post){
        pass_config config;config.kind=pass_kind::compute;config.depth_test=config.depth_write=false;
        config.stage=add_post ? pass_stage::post_processing : pass_stage::pre_processing;
        config.name=config.stage==pass_stage::pre_processing ? "Pre-processing" : "Post-processing";
        try{fs.get_scan().save_pass(fs.get_config().library_root/"generated/render_pass"/(config.guid+".pass"),config);fs.get_scan().save_project_config(config.guid);select_pass(config.guid);}catch(const std::exception& e){log(e.what());}
    }
    ImGui::SameLine();
    if(ImGui::Button("+ Shader")){
        try{
            const std::string source="#version 450\nlayout(local_size_x=1,local_size_y=1,local_size_z=1) in;\nvoid main(){}\n";
            sub_shader_config stage;stage.name="Compute Stage";stage.stage=shader_stage::compute;
            stage.source_guid=fs.get_scan().create_file(fs.get_config().source_root/"shaders"/(stage.guid+".glsl"),{reinterpret_cast<const uint8_t*>(source.data()),source.size()});
            fs.get_scan().save_sub_shader(fs.get_config().library_root/"generated/render_pass"/(stage.guid+".sub_shader"),stage);fs.get_scan().save_project_config(stage.guid);
            shader_config shader;shader.name="New Shader";shader.sub_shader_guids={stage.guid};
            fs.get_scan().save_shader(fs.get_config().library_root/"generated/render_pass"/(shader.guid+".shader"),shader);fs.get_scan().save_project_config(shader.guid);
            selected_asset=shader.guid;log("Created shader and compute-stage source. Select its stage in Project to edit.");
        }catch(const std::exception& e){log(e.what());}
    }
    ImGui::BeginChild("pass list",{300,0},true);
    std::vector<const pass_config*> passes;
    for(const auto& [guid,file]:fs.get_files())if(file.type==file_type::render_pass){const auto* pass=data.get_pass(guid);if(pass)passes.push_back(pass);}
    std::sort(passes.begin(),passes.end(),[](const auto* a,const auto* b){return a->order==b->order ? a->guid<b->guid : a->order<b->order;});
    for(const auto* pass:passes){ImGui::PushID(pass->guid.c_str());const auto label=pass->name+(pass->enabled ? "" : " (off)");
        if(ImGui::Selectable(label.c_str(),selected_pass==pass->guid)){selected_pass=pass->guid;pass_draft=*pass;}ImGui::PopID();}
    ImGui::EndChild();ImGui::SameLine();ImGui::BeginChild("pass properties",{0,0});
    if(!selected_pass.empty()){
        const bool editable=global_stage(pass_draft.stage);
        ImGui::TextWrapped("Pass GUID: %s",pass_draft.guid.c_str());
        ImGui::BeginDisabled(!editable);
        text_field("Name",pass_draft.name);ImGui::Checkbox("Enabled",&pass_draft.enabled);
        int stage=int(pass_draft.stage);const char* stages[]={"Pre-processing","Opaque","Transparent Capture","Transparent Sort","Transparent Resolve","Post-processing","Geometry","Skybox"};
        if(editable){int choice=stage==7 ? 2 : stage==5 ? 1 : 0;if(ImGui::Combo("Stage",&choice,"Pre-processing\0Post-processing\0Skybox\0"))pass_draft.stage=choice==2 ? pass_stage::skybox : choice==1 ? pass_stage::post_processing : pass_stage::pre_processing;}
        else ImGui::Text("Stage: %s",stages[stage]);
        int order=int(pass_draft.order);if(ImGui::InputInt("Order",&order))pass_draft.order=uint32_t(std::max(0,order));
        int kind=int(pass_draft.kind);const char* kinds[]={"Graphics","Compute"};if(ImGui::Combo("Pipeline",&kind,kinds,2))pass_draft.kind=pass_kind(kind);
        asset_combo("Shader",pass_draft.shader_guid,file_type::shader_config);
        ImGui::EndDisabled();
        ImGui::TextWrapped("Shader GUID: %s",pass_draft.shader_guid.empty() ? "null" : pass_draft.shader_guid.c_str());
        if(auto* shader=data.get_shader(pass_draft.shader_guid)){
            for(const auto& guid:shader->sub_shader_guids)if(auto* sub=data.get_sub_shader(guid)){
                ImGui::TextWrapped("Sub Shader: %s\nGUID: %s\nEntry: %s",sub->name.c_str(),sub->guid.c_str(),sub->entry_point.c_str());
                const auto file=fs.get_files().find(sub->source_guid);const auto source=!sub->builtin_source.empty() ? "Engine/shader/"+sub->builtin_source.generic_string() : file==fs.get_files().end() ? "null" : file->second.path.generic_string();ImGui::TextWrapped("Source: %s",source.c_str());
            }
            if(ImGui::Button("Inspect Shader"))selected_asset=shader->guid;
        }
        ImGui::Separator();
        if(editable && ImGui::Button("Save Configuration")){
            try{fs.get_scan().save_pass(fs.get_files().at(selected_pass).path,pass_draft);fs.get_scan().save_project_config(pass_draft.guid);log("Saved global pass: "+pass_draft.name);}catch(const std::exception& e){log(e.what());}
        }
        ImGui::TextWrapped(pass_draft.shader_guid.empty() ? "null: this stage has no implementation." : "Built-in bindings are loaded at renderer startup. Custom pass scheduling: null.");
    }else ImGui::TextWrapped("Select a pass, or create a pre/post-processing pass.");
    ImGui::EndChild();ImGui::End();
}
void editor::edit_asset(){
    auto& fs=egn.get_file_system();const auto found=fs.get_files().find(selected_asset);
    if(found==fs.get_files().end()){selected_asset.clear();return;}
    const auto info=found->second;
    ImGui::TextWrapped("%s",info.name.c_str());ImGui::TextDisabled("%s",info.guid.c_str());ImGui::Separator();
    ImGui::TextWrapped("%s",info.path.generic_string().c_str());
    auto& cache=fs.get_resources().get_render_pass_data();
    if(info.type==file_type::render_pass){
        if(auto* pass=cache.get_pass(info.guid);pass){if(ImGui::Button("Global Pass Settings"))select_pass(info.guid);}
        else ImGui::TextWrapped("Opaque / transparency pass: owned by the renderer.");
    }else if(info.type==file_type::shader_config){
        auto* config=cache.get_shader(info.guid);text_field("Name",config->name);
        std::string selected;
        for(size_t i=0;i<config->sub_shader_guids.size();++i){
            ImGui::PushID(int(i));asset_combo("Sub Shader",config->sub_shader_guids[i],file_type::sub_shader);
            ImGui::SameLine();if(ImGui::SmallButton("Edit"))selected=config->sub_shader_guids[i];ImGui::PopID();
        }
        if(ImGui::Button("+ Stage"))config->sub_shader_guids.emplace_back();
        if(ImGui::Button("Save Shader"))try{const auto saved=*config;fs.get_scan().save_shader(info.path,saved);fs.get_scan().save_project_config(saved.guid);log("Saved shader: "+saved.name);}catch(const std::exception& e){log(e.what());}
        if(!selected.empty())selected_asset=selected;
    }else if(info.type==file_type::sub_shader){
        auto* config=cache.get_sub_shader(info.guid);text_field("Name",config->name);
        if(!config->builtin_source.empty())ImGui::TextWrapped("Engine/shader/%s",config->builtin_source.generic_string().c_str());
        if(asset_combo("Source",config->source_guid,file_type::shader))config->builtin_source.clear();
        const char* names[]={"Vertex","Fragment","Geometry","Tess Control","Tess Evaluation","Compute"};
        int stage=int(config->stage);if(ImGui::Combo("Stage",&stage,names,6))config->stage=shader_stage(stage);
        text_field("Entry Point",config->entry_point);
        std::string defines;for(const auto& [key,value]:config->defines)defines+=key+"="+value+"\n";
        std::array<char,2048> buffer{};std::copy_n(defines.data(),std::min(defines.size(),buffer.size()-1),buffer.data());
        if(ImGui::InputTextMultiline("Defines",buffer.data(),buffer.size(),{0,100})){
            config->defines.clear();std::istringstream lines(buffer.data());std::string line;
            while(std::getline(lines,line)){if(line.empty())continue;const auto equal=line.find('=');config->defines[line.substr(0,equal)]=equal==std::string::npos ? "" : line.substr(equal+1);}
        }
        if(ImGui::Button("Save & Compile"))try{const auto saved=*config;fs.get_scan().save_sub_shader(info.path,saved);fs.get_scan().save_project_config(saved.guid);log("Compiled stage: "+saved.name);}catch(const std::exception& e){log(e.what());}
    }else if(info.type==file_type::object){
        if(ImGui::Button("Instantiate Object"))add_object_asset(info.guid);
    }else if(info.type==file_type::model){
        if(ImGui::Button("Add to Scene"))try{add_model(info.guid);}catch(const std::exception& e){log(e.what());}
    }else if(info.type==file_type::scene){if(ImGui::Button("Open Scene"))open_scene(fs.get_scan().get_source_path(info.guid));}
}
}
