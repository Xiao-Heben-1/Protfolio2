#include <file/config/scene_config.hpp>
#include <file/config/config_json.hpp>
namespace render {
using namespace config_json;
scene_config scene_config::load(const std::filesystem::path& path){
    scene_config result;auto document=config_json::load(path,result.guid);
    read_guid(document,result.guid);read(document,"version",result.version);read(document,"name",result.name);
    if(document.HasMember("main_camera"))result.main_camera.read(document["main_camera"]);
    if(document.HasMember("skybox"))result.skybox.read(document["skybox"]);
    if(document.HasMember("lights"))for(const auto& item:document["lights"].GetArray()){
        light_config value;value.read(item);result.lights.push_back(std::move(value));
    }
    if(document.HasMember("objects"))for(const auto& item:document["objects"].GetArray()){
        object_config value;value.read(item);result.objects.push_back(std::move(value));
    }
    return result;
}
void scene_config::save(const std::filesystem::path& path) const {
    rapidjson::Document document(rapidjson::kObjectType);auto& alloc=document.GetAllocator();
    put(document,"version",version,alloc);put(document,"name",name,alloc);
    document.AddMember("main_camera",main_camera.write(alloc),alloc);
    document.AddMember("skybox",skybox.write(alloc),alloc);
    json json_lights(rapidjson::kArrayType),json_objects(rapidjson::kArrayType);
    for(const auto& light:lights)json_lights.PushBack(light.write(alloc),alloc);
    for(const auto& object:objects)json_objects.PushBack(object.write(alloc),alloc);
    document.AddMember("lights",json_lights,alloc);document.AddMember("objects",json_objects,alloc);
    config_json::save(path,guid,document);
}
}
