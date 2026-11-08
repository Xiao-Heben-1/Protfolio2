#include <file/config/component/camera_config.hpp>
#include <file/config/config_json.hpp>
namespace render {
using namespace config_json;
json camera_config::write(allocator& alloc) const {
    json result(rapidjson::kObjectType);
    put(result,"is_main",is_main,alloc);
    put(result,"position",position,alloc);
    put(result,"direction",direction,alloc);
    put(result,"fov",fov,alloc);
    put(result,"aspect",aspect,alloc);
    put(result,"near_plane",near_plane,alloc);
    put(result,"far_plane",far_plane,alloc);
    put(result,"dis",dis,alloc);
    put(result,"navigation",navigation,alloc);
    put(result,"width",width,alloc);
    put(result,"height",height,alloc);
    put(result,"g_buffer_format",g_buffer_format,alloc);
    put(result,"depth_format",depth_format,alloc);
    return result;
}
void camera_config::read(const json& source){
    config_json::read(source,"is_main",is_main);
    config_json::read(source,"position",position);
    config_json::read(source,"direction",direction);
    config_json::read(source,"fov",fov);
    config_json::read(source,"aspect",aspect);
    config_json::read(source,"near_plane",near_plane);
    config_json::read(source,"far_plane",far_plane);
    config_json::read(source,"dis",dis);
    config_json::read(source,"navigation",navigation);
    config_json::read(source,"width",width);
    config_json::read(source,"height",height);
    config_json::read(source,"g_buffer_format",g_buffer_format);
    config_json::read(source,"depth_format",depth_format);
}
}
