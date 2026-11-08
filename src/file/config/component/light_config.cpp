#include <file/config/component/light_config.hpp>
#include <file/config/config_json.hpp>
namespace render {
using namespace config_json;
json light_config::write(allocator& alloc) const {
    json result(rapidjson::kObjectType);
    put(result,"guid",guid,alloc);put(result,"name",name,alloc);
    const std::string kind=type==light_type::main ? "main" : type==light_type::directional ? "directional" : "point";
    put(result,"type",kind,alloc);
    if(type==light_type::main){
        put(result,"direction",direction,alloc);put(result,"solar_irradiance",solar_irradiance,alloc);
        put(result,"angular_radius",angular_radius,alloc);put(result,"disk_intensity",disk_intensity,alloc);
    }else{
        put(result,"color",color,alloc);put(result,"emission",emission,alloc);
        if(type==light_type::point){put(result,"position",position,alloc);put(result,"k1",k1,alloc);put(result,"k2",k2,alloc);}
        else put(result,"direction",direction,alloc);
    }
    return result;
}
void light_config::read(const json& source){
    read_guid(source,guid);config_json::read(source,"name",name);
    std::string kind="point";config_json::read(source,"type",kind);
    type=kind=="main" ? light_type::main : kind=="directional" ? light_type::directional : light_type::point;
    config_json::read(source,"position",position);config_json::read(source,"direction",direction);config_json::read(source,"color",color);
    config_json::read(source,"emission",emission);config_json::read(source,"k1",k1);config_json::read(source,"k2",k2);
    if(type==light_type::main && !source.HasMember("solar_irradiance") && source.HasMember("emission"))
        for(size_t i=0;i<3;++i)solar_irradiance[i]=color[i]*emission;
    config_json::read(source,"solar_irradiance",solar_irradiance);
    config_json::read(source,"angular_radius",angular_radius);config_json::read(source,"disk_intensity",disk_intensity);
}
}
