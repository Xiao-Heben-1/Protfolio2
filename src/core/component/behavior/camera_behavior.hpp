#pragma once
#include <core/component/behavior/behavior.hpp>
#include <file/config/component/camera_config.hpp>
#include <glm/glm.hpp>
#include <optional>
namespace render {
class camera;
struct camera_input {
    glm::vec3 move{0}; // 相机局部空间：x右、y上、z前
    glm::vec2 look{0},pan{0}; // 鼠标像素增量
    float zoom=0,delta_time=0;
    bool fast=false;
    std::optional<camera_config> pose;
    std::optional<glm::vec3> focus;
    float focus_distance=10;
};
struct camera_navigation {
    float move_speed=10,look_speed=.0012f,pan_speed=.006f,zoom_speed=1,fast_multiplier=4;
};
// 主相机为独立组件，由 camera 持有本行为；不创建额外 object 或缓存 mesh/material。
class camera_behavior final:public behavior {
    friend class camera;
    camera& target;
    camera_input input;
    camera_navigation navigation;
public:
    camera_input& get_input() noexcept{return input;}
    camera_navigation& get_navigation() noexcept{return navigation;}
private:
    explicit camera_behavior(camera& target);
    ~camera_behavior() override=default;
    void update() override;
};
}
