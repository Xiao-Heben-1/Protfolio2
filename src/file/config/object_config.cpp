#include <file/config/object_config.hpp>
#include <file/config/config_json.hpp>
namespace render {
using namespace config_json;
json object_config::write(allocator& alloc) const {
    json result(rapidjson::kObjectType);
    put(result,"version",version,alloc);put(result,"guid",guid,alloc);put(result,"name",name,alloc);
    put(result,"is_transparent",is_transparent,alloc);
    put(result,"behavior",behavior,alloc);
    result.AddMember("transform",transform.write(alloc),alloc);
    if(mesh)result.AddMember("mesh",mesh->write(alloc),alloc);
    if(material)result.AddMember("material",material->write(alloc),alloc);
    if(camera)result.AddMember("camera",camera->write(alloc),alloc);
    json values(rapidjson::kArrayType);
    for(const auto& light:lights)values.PushBack(light.write(alloc),alloc);
    result.AddMember("lights",values,alloc);
    return result;
}
void object_config::read(const json& source){
    read_guid(source,guid);config_json::read(source,"version",version);config_json::read(source,"name",name);
    config_json::read(source,"behavior",behavior);
    if(source.HasMember("camera")){camera.emplace();camera->read(source["camera"]);}
    if(source.HasMember("lights"))for(const auto& value:source["lights"].GetArray()){
        light_config light;light.read(value);lights.push_back(light);
    }
    config_json::read(source,source.HasMember("is_transparent") ? "is_transparent" : "IsTransparent",is_transparent);
    if(source.HasMember("transform"))transform.read(source["transform"]);
    if(source.HasMember("mesh")){mesh.emplace();mesh->read(source["mesh"]);}
    else if(source.HasMember("mesh_uid")){mesh.emplace();config_json::read(source,"mesh_uid",mesh->source_guid);}
    if(source.HasMember("material")){material.emplace();material->read(source["material"]);}
    else if(source.HasMember("material_uids") || source.HasMember("material_overrides")){
        material.emplace();material->read_legacy(source);
    }
}
object_config object_config::load(const std::filesystem::path& path){
    object_config result;auto document=config_json::load(path,result.guid);result.read(document);return result;
}
void object_config::save(const std::filesystem::path& path) const {
    rapidjson::Document document;auto value=write(document.GetAllocator());
    value.RemoveMember("guid"); // 独立文件GUID写在首行；嵌入场景时由write保留GUID字段。
    config_json::save(path,guid,value);
}
}
