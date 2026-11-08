#include <core/component/behavior/camera_behavior.hpp>
#include <core/component/camera/camera.hpp>
#include <algorithm>
#include <cmath>
namespace render {
camera_behavior::camera_behavior(camera& cam):behavior(nullptr),target(cam){}
void camera_behavior::update(){
    if(input.pose)target.apply_config(*input.pose);
    auto forward=glm::normalize(target.direction);
    float yaw=std::atan2(forward.x,-forward.z)+input.look.x*navigation.look_speed;
    float pitch=std::clamp(std::asin(forward.y)-input.look.y*navigation.look_speed,-1.55f,1.55f);
    forward={std::sin(yaw)*std::cos(pitch),std::sin(pitch),-std::cos(yaw)*std::cos(pitch)};
    const auto right=glm::normalize(glm::cross(forward,glm::vec3(0,1,0)));
    const auto up=glm::normalize(glm::cross(right,forward));
    auto move=input.move;
    if(glm::dot(move,move)>1)move=glm::normalize(move);
    target.position+=(right*move.x+glm::vec3(0,1,0)*move.y+forward*move.z)*
        navigation.move_speed*input.delta_time*(input.fast ? navigation.fast_multiplier : 1);
    target.position+=(-right*input.pan.x+up*input.pan.y)*navigation.pan_speed;
    target.position+=forward*input.zoom*navigation.zoom_speed;
    if(input.focus)target.position=*input.focus-forward*input.focus_distance;
    target.direction=forward;
    input={};
}
}
