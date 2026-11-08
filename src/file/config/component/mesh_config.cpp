#include <file/config/component/mesh_config.hpp>
#include <file/config/config_json.hpp>
namespace render {
using namespace config_json;
json mesh_config::write(allocator& alloc) const {
    json result(rapidjson::kObjectType);
    put(result,"builtin_geometry",builtin_geometry,alloc);
    put(result,"source_guid",source_guid,alloc);put(result,"lod_guids",lod_guids,alloc);
    put_optional(result,"bounds",bounds,alloc);return result;
}
void mesh_config::read(const json& source){
    config_json::read(source,"builtin_geometry",builtin_geometry);
    config_json::read(source,"source_guid",source_guid);config_json::read(source,"lod_guids",lod_guids);
    read_optional(source,"bounds",bounds);
}
}
