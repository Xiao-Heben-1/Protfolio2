#include <file/config/render_pass/sub_shader_config.hpp>
#include <file/config/config_json.hpp>
#include <algorithm>
#include <file/tools/file_path.hpp>
namespace render {
using namespace config_json;
namespace {constexpr const char* stages[]={"vertex","fragment","geometry","tess_control","tess_evaluation","compute"};}
sub_shader_config sub_shader_config::load(const std::filesystem::path& path){
    sub_shader_config result;auto json=config_json::load(path,result.guid);
    read(json,"version",result.version);read(json,"name",result.name);read(json,"source_guid",result.source_guid);read(json,"entry_point",result.entry_point);
    if(json.HasMember("builtin_source"))result.builtin_source=path_from_utf8(json["builtin_source"].GetString());
    if(json.HasMember("stage"))result.stage=shader_stage(std::find(std::begin(stages),std::end(stages),std::string(json["stage"].GetString()))-std::begin(stages));
    if(json.HasMember("defines"))for(auto item=json["defines"].MemberBegin();item!=json["defines"].MemberEnd();++item)result.defines[item->name.GetString()]=item->value.GetString();
    if(json.HasMember("include_roots"))for(const auto& item:json["include_roots"].GetArray())result.include_roots.push_back(path_from_utf8(item.GetString()));
    return result;
}
void sub_shader_config::save(const std::filesystem::path& path) const{
    rapidjson::Document json(rapidjson::kObjectType);auto& alloc=json.GetAllocator();
    put(json,"version",version,alloc);put(json,"name",name,alloc);put(json,"source_guid",source_guid,alloc);
    put(json,"builtin_source",path_to_utf8(builtin_source),alloc);
    put(json,"entry_point",entry_point,alloc);put(json,"stage",std::string(stages[size_t(stage)]),alloc);
    config_json::json macros(rapidjson::kObjectType),includes(rapidjson::kArrayType);
    for(const auto& [key,value]:defines)put(macros,key.c_str(),value,alloc);
    for(const auto& path:include_roots){const auto u8=path.generic_u8string();includes.PushBack(encode(std::string(u8.begin(),u8.end()),alloc),alloc);}
    json.AddMember("defines",macros,alloc);json.AddMember("include_roots",includes,alloc);config_json::save(path,guid,json);
}
}
