#pragma once
#include <array>
#include <rapidjson/document.h>
namespace render {
struct transform_config {
    std::array<float,3> position{0,0,0};
    std::array<float,3> rotation{0,0,0}; // 欧拉角，弧度。
    std::array<float,3> scale{1,1,1};
private:
    friend struct object_config;
    rapidjson::Value write(rapidjson::Document::AllocatorType& alloc) const;
    void read(const rapidjson::Value& source);
};
}
