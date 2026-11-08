#include <file/config/component/transform_config.hpp>
#include <file/config/config_json.hpp>
namespace render {
using namespace config_json;
json transform_config::write(allocator& alloc) const {
    json result(rapidjson::kObjectType);
    put(result,"position",position,alloc);
    put(result,"rotation",rotation,alloc);
    put(result,"scale",scale,alloc);
    return result;
}
void transform_config::read(const json& source){
    config_json::read(source,"position",position);
    config_json::read(source,"rotation",rotation);
    config_json::read(source,"scale",scale);
}
}
