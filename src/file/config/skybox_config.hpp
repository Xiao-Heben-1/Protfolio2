#pragma once
#include <array>
#include <rapidjson/document.h>
namespace render {
// 场景大气参数；单位与 skybox_data 一致，不保存太阳和 GPU 句柄。
struct skybox_config {
    float earth_radius=6371000.0f;
    float atmosphere_height=100000.0f;
    float rayleigh_scale_height=8000.0f;
    float mie_scale_height=1200.0f;
    std::array<float,3> rayleigh_scattering{5.802e-6f,13.558e-6f,33.100e-6f};
    std::array<float,3> mie_scattering{3.996e-6f,3.996e-6f,3.996e-6f};
    std::array<float,3> mie_extinction{4.440e-6f,4.440e-6f,4.440e-6f};
    float mie_g=0.8f;
    std::array<float,3> ozone_absorption{0.650e-6f,1.881e-6f,0.085e-6f};
    float ozone_center_height=25000.0f;
    float ozone_half_width=15000.0f;
    float exposure=1.0f;
    std::array<float,3> ground_albedo{0.3f,0.3f,0.3f};
    float ground_intensity=1.0f;
    std::array<float,3> ground_color{0.15f,0.15f,0.15f};
    float atmosphere_density=1.0f;
    uint32_t view_sample_count=16;
    uint32_t light_sample_count=8;
private:
    friend struct scene_config;
    rapidjson::Value write(rapidjson::Document::AllocatorType& alloc) const;
    void read(const rapidjson::Value& source);
};
}
