#include <file/config/skybox_config.hpp>
#include <file/config/config_json.hpp>
namespace render {
using namespace config_json;
json skybox_config::write(allocator& alloc) const{
    json result(rapidjson::kObjectType);
    put(result,"earth_radius",earth_radius,alloc);
    put(result,"atmosphere_height",atmosphere_height,alloc);
    put(result,"rayleigh_scale_height",rayleigh_scale_height,alloc);
    put(result,"mie_scale_height",mie_scale_height,alloc);
    put(result,"rayleigh_scattering",rayleigh_scattering,alloc);
    put(result,"mie_scattering",mie_scattering,alloc);
    put(result,"mie_extinction",mie_extinction,alloc);
    put(result,"mie_g",mie_g,alloc);
    put(result,"ozone_absorption",ozone_absorption,alloc);
    put(result,"ozone_center_height",ozone_center_height,alloc);
    put(result,"ozone_half_width",ozone_half_width,alloc);
    put(result,"exposure",exposure,alloc);
    put(result,"ground_albedo",ground_albedo,alloc);
    put(result,"ground_intensity",ground_intensity,alloc);
    put(result,"ground_color",ground_color,alloc);
    put(result,"atmosphere_density",atmosphere_density,alloc);
    put(result,"view_sample_count",view_sample_count,alloc);
    put(result,"light_sample_count",light_sample_count,alloc);
    return result;
}
void skybox_config::read(const json& source){
    config_json::read(source,"earth_radius",earth_radius);
    config_json::read(source,"atmosphere_height",atmosphere_height);
    config_json::read(source,"rayleigh_scale_height",rayleigh_scale_height);
    config_json::read(source,"mie_scale_height",mie_scale_height);
    config_json::read(source,"rayleigh_scattering",rayleigh_scattering);
    config_json::read(source,"mie_scattering",mie_scattering);
    config_json::read(source,"mie_extinction",mie_extinction);
    config_json::read(source,"mie_g",mie_g);
    config_json::read(source,"ozone_absorption",ozone_absorption);
    config_json::read(source,"ozone_center_height",ozone_center_height);
    config_json::read(source,"ozone_half_width",ozone_half_width);
    config_json::read(source,"exposure",exposure);
    config_json::read(source,"ground_albedo",ground_albedo);
    config_json::read(source,"ground_intensity",ground_intensity);
    config_json::read(source,"ground_color",ground_color);
    config_json::read(source,"atmosphere_density",atmosphere_density);
    config_json::read(source,"view_sample_count",view_sample_count);
    config_json::read(source,"light_sample_count",light_sample_count);
}
}
