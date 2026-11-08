#pragma once
#include <array>
#include <cstdint>
#include <vulkan/vulkan.h>
#include <rapidjson/document.h>
namespace render {
struct camera_config {
    bool is_main=false;
    std::array<float,3> position{0,0,0};
    std::array<float,3> direction{0,0,-1};
    float fov=45; // 角度。
    float aspect=16.f/9.f;
    float near_plane=.1f;
    float far_plane=1152.f;
    std::array<float,4> dis{128,512,512,1024}; // 四段宽度，距离按世界AABB最近点计算。
    std::array<float,5> navigation{10,.0012f,.006f,1,4}; // 移动、转向、平移、缩放、加速。
    uint32_t width=1280,height=720;
    uint32_t g_buffer_format=VK_FORMAT_R16G16B16A16_SFLOAT;
    uint32_t depth_format=VK_FORMAT_UNDEFINED; // 初始化设备时选择。
private:
    friend struct scene_config;
    friend struct object_config;
    rapidjson::Value write(rapidjson::Document::AllocatorType& alloc) const;
    void read(const rapidjson::Value& source);
};
}
