#include <front/editor.hpp>
#include <core/object/object_loader.hpp>
#include <file/tools/library_cache.hpp>
#include <file/tools/file_path.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <fstream>
#include <algorithm>
namespace render::front {
void editor::add_object(object_config config){
    std::unique_ptr<object> runtime(object_loader::load(config));
    objects.push_back({std::move(config),std::move(runtime)});selected_object=int(objects.size())-1;camera_selected=false;lighting_selected=false;selected_asset.clear();scene_dirty=true;
}
void editor::add_model(const std::string& guid){
    const auto info=egn.get_file_system().get_files().at(guid);
    object_config config;config.name=path_to_utf8(info.path.stem());config.mesh.emplace();config.mesh->source_guid=guid;
    add_object(std::move(config));
    auto& item=objects.back();item.config=object_loader::describe(*item.runtime,item.config);
    egn.get_file_system().get_scan().save_object(item.config.guid,egn.get_file_system().get_config().library_root/"generated/objects"/(item.config.guid+".object"),item.config);
    item.runtime->update();focus_selected();log("Converted model to Object: "+info.name);
}
void editor::open_scene(const std::filesystem::path& path){
    auto loaded=scene_config::load(egn.get_file_system().get_config().library_root/path);
    // 先读取配置，再切换当前编辑场景。
    vkDeviceWaitIdle(gpu.device);objects.clear();selected_object=-1;
    scene_document=std::move(loaded);scene_path=(egn.get_file_system().get_config().library_root/path).lexically_normal();
    egn.get_render_system().get_skybox_data().apply_config(scene_document.skybox);
    for(const auto& config:scene_document.objects)try{add_object(config);}catch(const std::exception& e){
        log(config.name+": "+e.what());
        auto placeholder=config;placeholder.mesh.reset();add_object(placeholder);objects.back().config=config;
    }
    // 旧版场景的独立光源迁移为普通 Object 组件。
    for(const auto& light:scene_document.lights){object_config config;config.name=light.name;config.lights.push_back(light);add_object(config);}
    scene_document.lights.clear();
    auto pose=scene_document.main_camera;
    pose.aspect=float(requested_extent.width)/float(requested_extent.height);
    const bool owned=std::any_of(objects.begin(),objects.end(),[](const auto& item){return item.runtime->get_camera() && item.runtime->get_camera()->get_is_main();});
    if(!owned)egn.get_render_system().get_scene().get_main_camera().get_behavior().get_input().pose=pose;
    camera_selected=true;lighting_selected=false;selected_object=-1;scene_dirty=false;
    log("Opened scene: "+path.string());
}
void editor::save_scene(bool authored){
    scene_document.objects.clear();scene_document.lights.clear();
    scene_document.skybox=egn.get_render_system().get_skybox_data().get_config();
    scene_document.main_camera=egn.get_render_system().get_scene().get_main_camera().get_config();
    for(auto& item:objects){
        item.config=object_loader::describe(*item.runtime,item.config);
        scene_document.objects.push_back(item.config);
    }
    egn.get_file_system().get_scan().save_scene(scene_document.guid,scene_path,scene_document);
    if(authored)egn.get_file_system().get_scan().save_project_config(scene_document.guid);
    scene_dirty=false;log("Saved scene: "+scene_path.string());
}
void editor::new_scene(){
    const auto& workspace=egn.get_file_system().get_config();
    open_scene(workspace.engine_root/"config/system/scenes/default.scene");
    scene_document.guid=generate_guid();
    for(auto& item:objects)item.config.guid=generate_guid();
    scene_path=workspace.library_root/"generated/scenes"/(scene_document.guid+".scene");
    scene_dirty=true;log("Created scene from engine definition");
}
void editor::focus_selected(){
    scene_dirty=true;
    auto& input=egn.get_render_system().get_scene().get_main_camera().get_behavior().get_input();
    if(selected_object>=0){
        const auto& item=objects[size_t(selected_object)];const auto* mesh=item.runtime->get_mesh();
        if(mesh && (!mesh->get_lods()[0].guid.empty() || !mesh->get_data().sub_meshes.empty())){const auto bounds=mesh->get_bounds().transformed(item.runtime->get_transform()->get_matrix());
            input.focus=(bounds.get_min()+bounds.get_max())*.5f;input.focus_distance=std::max(2.f,glm::length(bounds.get_max()-bounds.get_min())*1.5f);
        }else{input.focus=item.runtime->get_transform()->position;input.focus_distance=10;}
    }else{input.focus=glm::vec3(0);input.focus_distance=12;}
}
void editor::import_path(const std::filesystem::path& source){
    if(std::filesystem::is_directory(source)){
        for(const auto& entry:std::filesystem::recursive_directory_iterator(source))
            if(entry.path().extension()==".fbx" || entry.path().extension()==".obj")load_model(entry.path());
    }else load_model(source);
}
void editor::load_model(const std::filesystem::path& source){
    std::unique_ptr<object> runtime(object_loader::load_model(source));
    object_config config;config.name=path_to_utf8(source.stem());config=object_loader::describe(*runtime,config);
    auto& fs=egn.get_file_system();
    fs.get_scan().save_object(config.guid,fs.get_config().library_root/"generated/objects"/(config.guid+".object"),config);
    objects.push_back({std::move(config),std::move(runtime)});selected_object=int(objects.size())-1;
    camera_selected=false;lighting_selected=false;selected_asset.clear();scene_dirty=true;
    objects.back().runtime->update();focus_selected();log("Loaded model: "+path_to_utf8(source.filename()));
}
}
