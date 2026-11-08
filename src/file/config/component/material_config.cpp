#include <file/config/component/material_config.hpp>
#include <file/config/config_json.hpp>
namespace render {
using namespace config_json;
json material_override_config::write(allocator& alloc) const {
    json result(rapidjson::kObjectType);put(result,"slot",slot,alloc);
    put_optional(result,"base_color",base_color,alloc);put_optional(result,"emissive_color",emissive_color,alloc);
    put_optional(result,"metallic",metallic,alloc);put_optional(result,"roughness",roughness,alloc);
    put_optional(result,"ao",ao,alloc);put_optional(result,"emissive_strength",emissive_strength,alloc);
    put_optional(result,"alpha_mode",alpha_mode,alloc);put_optional(result,"alpha_cutoff",alpha_cutoff,alloc);
    put_optional(result,"base_color_alpha",base_color_alpha,alloc);put_optional(result,"visible",visible,alloc);
    put_optional(result,"texture_channels",texture_channels,alloc);
    put_optional(result,"shader_guid",shader_guid,alloc);
    json maps(rapidjson::kObjectType);
    for(const auto& [slot,guid]:textures)put(maps,slot.c_str(),guid,alloc);
    result.AddMember("textures",maps,alloc);return result;
}
void material_override_config::read(const json& source){
    config_json::read(source,"slot",slot);
    read_optional(source,"base_color",base_color);read_optional(source,"emissive_color",emissive_color);
    read_optional(source,"metallic",metallic);read_optional(source,"roughness",roughness);
    read_optional(source,"ao",ao);read_optional(source,"emissive_strength",emissive_strength);
    read_optional(source,"alpha_mode",alpha_mode);read_optional(source,"alpha_cutoff",alpha_cutoff);
    read_optional(source,"base_color_alpha",base_color_alpha);read_optional(source,"visible",visible);
    read_optional(source,"texture_channels",texture_channels);
    read_optional(source,source.HasMember("shader_guid") ? "shader_guid" : "shader_uid",shader_guid);
    if(source.HasMember("textures"))for(auto item=source["textures"].MemberBegin();item!=source["textures"].MemberEnd();++item)
        textures.emplace(std::string(item->name.GetString(),item->name.GetStringLength()),std::string(item->value.GetString(),item->value.GetStringLength()));
}
json material_config::write(allocator& alloc) const {
    json result(rapidjson::kObjectType),materials(rapidjson::kArrayType),values(rapidjson::kArrayType);
    for(const auto& guid:guids)materials.PushBack(encode(guid,alloc),alloc);
    for(const auto& value:overrides)values.PushBack(value.write(alloc),alloc);
    result.AddMember("guids",materials,alloc);result.AddMember("overrides",values,alloc);return result;
}
void material_config::read(const json& source){
    if(source.HasMember("guids"))for(const auto& item:source["guids"].GetArray())guids.emplace_back(item.GetString(),item.GetStringLength());
    if(source.HasMember("overrides"))for(const auto& item:source["overrides"].GetArray()){
        material_override_config value;value.read(item);overrides.push_back(std::move(value));
    }
}
void material_config::read_legacy(const json& source){
    if(source.HasMember("material_uids"))for(const auto& item:source["material_uids"].GetArray())guids.emplace_back(item.GetString(),item.GetStringLength());
    if(source.HasMember("material_overrides"))for(const auto& item:source["material_overrides"].GetArray()){
        material_override_config value;value.read(item);overrides.push_back(std::move(value));
    }
}
}
