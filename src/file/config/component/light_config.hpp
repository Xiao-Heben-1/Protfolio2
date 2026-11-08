#pragma once
#include <array>
#include <string>
#include <file/tools/guid.hpp>
#include <rapidjson/document.h>
namespace render {
enum class light_type { main,point,directional };
struct light_config {
    std::string guid=generate_guid();
    std::string name;
    light_type type=light_type::point;
    std::array<float,3> position{0,0,0};
    std::array<float,3> direction{0,-1,0};
    std::array<float,3> color{1,1,1};
    float emission=1,k1=.7f,k2=1.8f;
    // main 专用：太阳法向辐照度（线性 RGB，W/m²），不随距离衰减。
    std::array<float,3> solar_irradiance{20,20,20};
    float angular_radius=.004675f; // 太阳角半径，rad。
    float disk_intensity=1; // 仅调节可见圆盘亮度。
private:
    friend struct scene_config;
    friend struct object_config;
    rapidjson::Value write(rapidjson::Document::AllocatorType& alloc) const;
    void read(const rapidjson::Value& source);
};
}
