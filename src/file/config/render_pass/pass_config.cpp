#include <file/config/render_pass/pass_config.hpp>
#include <file/config/config_json.hpp>
#include <algorithm>
namespace render {
using namespace config_json;
namespace {
constexpr const char* stages[]={"pre_processing","opaque","transparent_capture","transparent_sort","transparent_resolve","post_processing","geometry_pass","skybox"};
template<class T> void read_state(const json& object,const char* key,T& value){
    if(object.HasMember(key))value=static_cast<T>(object[key].GetUint());
}
}
pass_config pass_config::load(const std::filesystem::path& path){
    pass_config result;auto json=config_json::load(path,result.guid);
    read(json,"version",result.version);read(json,"name",result.name);read(json,"shader_guid",result.shader_guid);
    read(json,"builtin",result.builtin);
    read(json,"enabled",result.enabled);read(json,"order",result.order);
    if(json.HasMember("stage"))result.stage=pass_stage(std::find(std::begin(stages),std::end(stages),std::string(json["stage"].GetString()))-std::begin(stages));
    if(json.HasMember("kind"))result.kind=std::string(json["kind"].GetString())=="compute" ? pass_kind::compute : pass_kind::graphics;
    read_state(json,"topology",result.topology);read_state(json,"polygon_mode",result.polygon_mode);
    read_state(json,"cull_mode",result.cull_mode);read_state(json,"front_face",result.front_face);
    read(json,"depth_test",result.depth_test);read(json,"depth_write",result.depth_write);read_state(json,"depth_compare",result.depth_compare);
    if(json.HasMember("color_blends"))for(const auto& item:json["color_blends"].GetArray()){
        pass_blend_config blend;read(item,"enabled",blend.enabled);
        read_state(item,"src_color",blend.src_color);read_state(item,"dst_color",blend.dst_color);read_state(item,"color_op",blend.color_op);
        read_state(item,"src_alpha",blend.src_alpha);read_state(item,"dst_alpha",blend.dst_alpha);read_state(item,"alpha_op",blend.alpha_op);
        read_state(item,"write_mask",blend.write_mask);result.color_blends.push_back(blend);
    }
    return result;
}
void pass_config::save(const std::filesystem::path& path) const{
    rapidjson::Document json(rapidjson::kObjectType);auto& alloc=json.GetAllocator();
    put(json,"version",version,alloc);put(json,"name",name,alloc);
    put(json,"builtin",builtin,alloc);
    if(shader_guid.empty())json.AddMember("shader_guid",rapidjson::Value(),alloc);else put(json,"shader_guid",shader_guid,alloc);
    put(json,"enabled",enabled,alloc);put(json,"order",order,alloc);
    put(json,"stage",std::string(stages[size_t(stage)]),alloc);
    put(json,"kind",std::string(kind==pass_kind::compute ? "compute" : "graphics"),alloc);
    put(json,"topology",uint32_t(topology),alloc);put(json,"polygon_mode",uint32_t(polygon_mode),alloc);
    put(json,"cull_mode",uint32_t(cull_mode),alloc);put(json,"front_face",uint32_t(front_face),alloc);
    put(json,"depth_test",depth_test,alloc);put(json,"depth_write",depth_write,alloc);put(json,"depth_compare",uint32_t(depth_compare),alloc);
    config_json::json blends(rapidjson::kArrayType);
    for(const auto& blend:color_blends){
        config_json::json item(rapidjson::kObjectType);
        put(item,"enabled",blend.enabled,alloc);put(item,"src_color",uint32_t(blend.src_color),alloc);
        put(item,"dst_color",uint32_t(blend.dst_color),alloc);put(item,"color_op",uint32_t(blend.color_op),alloc);
        put(item,"src_alpha",uint32_t(blend.src_alpha),alloc);put(item,"dst_alpha",uint32_t(blend.dst_alpha),alloc);
        put(item,"alpha_op",uint32_t(blend.alpha_op),alloc);put(item,"write_mask",uint32_t(blend.write_mask),alloc);
        blends.PushBack(item,alloc);
    }
    json.AddMember("color_blends",blends,alloc);config_json::save(path,guid,json);
}
}
