#include <core/component/camera/camera.hpp>
#include <engine.hpp>
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/euler_angles.hpp>
namespace render {
camera::camera(float ratio,glm::vec3 p,glm::vec3 d,float f,float n,float far):component(nullptr,-1),position(p),direction(d),fov(f),aspect(ratio),near_plane(n),far_plane(far),navigation(*this){update_projection();update_view();}
void camera::update_projection() noexcept {projection=glm::perspective(glm::radians(fov),aspect,near_plane,far_plane);projection[1][1]*=-1;vp=projection*view;}
void camera::update_view() noexcept {view=glm::lookAt(position,position+direction,glm::vec3(0,1,0));vp=projection*view;view_frustum=frustum(vp);}
camera::camera(object* value):component(value,uint32_t(component_type::camera_type)),
    position(0),direction(0,0,-1),fov(45),aspect(16.f/9),near_plane(.1f),far_plane(1152),navigation(*this){
    owner->cam=this;scene=&egn.get_render_system().get_scene();update();
}
camera::~camera(){if(scene && is_main && this!=&scene->main_camera)scene->select_camera(scene->main_camera);}
void camera::set_main(){scene->select_camera(*this);}
void camera::update() noexcept {
    if(owner){
        const auto* tran=owner->get_transform();position=tran->position;
        direction=glm::vec3(glm::eulerAngleXYZ(tran->rotation.x,tran->rotation.y,tran->rotation.z)*glm::vec4(0,0,-1,0));
    }
    const bool turn=navigation.get_input().pose.has_value() || navigation.get_input().look!=glm::vec2(0);
    navigation.update();
    if(owner){
        auto* tran=owner->get_transform();tran->position=position;
        if(turn){const auto rotation=glm::inverse(glm::lookAt(glm::vec3(0),direction,glm::vec3(0,1,0)));
            glm::extractEulerAngleXYZ(rotation,tran->rotation.x,tran->rotation.y,tran->rotation.z);}
    }
    update_projection();update_view();
}
camera_config camera::get_config() const{
    camera_config config;config.is_main=is_main;
    config.position={position.x,position.y,position.z};config.direction={direction.x,direction.y,direction.z};
    config.fov=fov;config.aspect=aspect;config.near_plane=near_plane;config.far_plane=far_plane;config.dis=dis;
    config.width=extent.width;config.height=extent.height;
    const auto& settings=navigation.navigation;
    config.navigation={settings.move_speed,settings.look_speed,settings.pan_speed,settings.zoom_speed,settings.fast_multiplier};
    config.g_buffer_format=g_buffer_format;config.depth_format=depth_format;
    return config;
}
void camera::apply_config(const camera_config& config){
    position={config.position[0],config.position[1],config.position[2]};
    direction=glm::normalize(glm::vec3(config.direction[0],config.direction[1],config.direction[2]));
    fov=config.fov;aspect=config.aspect;near_plane=config.near_plane;far_plane=config.far_plane;dis=config.dis;
    navigation.navigation={config.navigation[0],config.navigation[1],config.navigation[2],config.navigation[3],config.navigation[4]};
}
camera& camera::operator=(const camera& o) noexcept {
    position=o.position;direction=o.direction;fov=o.fov;aspect=o.aspect;near_plane=o.near_plane;far_plane=o.far_plane;
    view=o.view;projection=o.projection;vp=o.vp;view_frustum=o.view_frustum;extent=o.extent;g_buffer_format=o.g_buffer_format;depth_format=o.depth_format;return *this;
}
const glm::vec3& camera::get_position()const noexcept{return position;}
const glm::mat4& camera::get_view()const noexcept{return view;}
const glm::mat4& camera::get_projection()const noexcept{return projection;}
const glm::mat4& camera::get_vp()const noexcept{return vp;}
const frustum& camera::get_frustum()const noexcept{return view_frustum;}
void camera::set_position(const glm::vec3& v)noexcept{position=v;}
void camera::set_direction(const glm::vec3& v)noexcept{direction=v;}
}
