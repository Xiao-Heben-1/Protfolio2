#include <file/config/render_pass/shader_config.hpp>
#include <file/config/config_json.hpp>
namespace render {
using namespace config_json;
shader_config shader_config::load(const std::filesystem::path& path){
    shader_config result;auto json=config_json::load(path,result.guid);
    read(json,"version",result.version);read(json,"name",result.name);
    if(json.HasMember("sub_shader_guids"))for(const auto& item:json["sub_shader_guids"].GetArray())
        result.sub_shader_guids.emplace_back(item.IsNull() ? "" : item.GetString());
    return result;
}
void shader_config::save(const std::filesystem::path& path) const{
    rapidjson::Document json(rapidjson::kObjectType);auto& alloc=json.GetAllocator();
    put(json,"version",version,alloc);put(json,"name",name,alloc);
    config_json::json stages(rapidjson::kArrayType);
    for(const auto& guid:sub_shader_guids)stages.PushBack(guid.empty() ? config_json::json() : encode(guid,alloc),alloc);
    json.AddMember("sub_shader_guids",stages,alloc);config_json::save(path,guid,json);
}
}
