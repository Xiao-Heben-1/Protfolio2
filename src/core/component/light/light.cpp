#include <core/component/light/light.hpp>
#include <engine.hpp>
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/euler_angles.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <algorithm>
namespace render {
namespace {std::array<float,3> array(glm::vec3 v){return {v.x,v.y,v.z};}}
light::light(object* value,component_type type):component(value,uint32_t(type)){}
void light::apply_config(const light_config& c){guid=c.guid;name=c.name;}
light_config light::get_config() const{light_config c;c.guid=guid;c.name=name;return c;}
glm::vec3 light::world_direction(glm::vec3 local) const{
    const auto r=owner->get_transform()->rotation;
    return glm::normalize(glm::vec3(glm::eulerAngleXYZ(r.x,r.y,r.z)*glm::vec4(local,0)));
}
main_light::main_light(object* value):light(value,component_type::main_light_type){
    auto& manager=egn.get_render_system().get_scene().get_light_manger();manager.main_lights.push_back(this);manager.main=this;
}
main_light::~main_light(){
    auto& manager=egn.get_render_system().get_scene().get_light_manger();
    std::erase(manager.main_lights,this);manager.main=manager.main_lights.empty() ? nullptr : manager.main_lights.back();
    if(manager.main)manager.main->update();
    else egn.get_render_system().get_light_data().main_light_data.solar_irradiance=glm::vec4(0);
}
glm::vec3 main_light::get_direction() const{return world_direction(direction);}
glm::vec3 main_light::get_solar_irradiance() const{return solar_irradiance;}
float main_light::get_angular_radius() const{return angular_radius;}
float main_light::get_disk_intensity() const{return disk_intensity;}
void main_light::apply_config(const light_config& c){
    light::apply_config(c);direction=glm::make_vec3(c.direction.data());solar_irradiance=glm::make_vec3(c.solar_irradiance.data());angular_radius=c.angular_radius;disk_intensity=c.disk_intensity;
}
light_config main_light::get_config() const{
    auto c=light::get_config();c.type=light_type::main;c.direction=array(direction);c.solar_irradiance=array(solar_irradiance);c.angular_radius=angular_radius;c.disk_intensity=disk_intensity;return c;
}
void main_light::update() noexcept{
    if(egn.get_render_system().get_scene().get_light_manger().main!=this)return;
    auto& data=egn.get_render_system().get_light_data().main_light_data;
    data.direction=glm::vec4(-get_direction(),angular_radius);
    data.solar_irradiance=glm::vec4(solar_irradiance,disk_intensity);
}
normal_light::normal_light(object* value,component_type type):light(value,type){
    auto& manager=egn.get_render_system().get_scene().get_light_manger();auto& data=egn.get_render_system().get_light_data();
    idx=int(manager.lights.size());manager.lights.push_back(this);data.light_datas.emplace_back();data.light_indices.push_back(idx);
}
normal_light::~normal_light(){
    auto& manager=egn.get_render_system().get_scene().get_light_manger();auto& data=egn.get_render_system().get_light_data();
    manager.lights[idx]=manager.lights.back();manager.lights[idx]->idx=idx;manager.lights.pop_back();
    data.light_datas[idx]=data.light_datas.back();data.light_datas.pop_back();data.light_indices.pop_back();
}
void normal_light::apply_config(const light_config& c){light::apply_config(c);color=glm::make_vec3(c.color.data());emission=c.emission;}
light_config normal_light::get_config() const{auto c=light::get_config();c.color=array(color);c.emission=emission;return c;}
glm::vec3 normal_light::get_color() const{return color;}
float normal_light::get_emission() const{return emission;}
point_light::point_light(object* value):normal_light(value,component_type::point_light_type){}
glm::vec3 point_light::get_position() const{return owner->get_transform()->position+position;}
float point_light::get_k1() const{return k1;}
float point_light::get_k2() const{return k2;}
void point_light::apply_config(const light_config& c){normal_light::apply_config(c);position=glm::make_vec3(c.position.data());k1=c.k1;k2=c.k2;}
light_config point_light::get_config() const{auto c=normal_light::get_config();c.type=light_type::point;c.position=array(position);c.k1=k1;c.k2=k2;return c;}
void point_light::update() noexcept{
    auto& data=egn.get_render_system().get_light_data().light_datas[idx];
    data.position=glm::vec4(get_position(),1);data.direction=glm::vec4(0);
    data.color=glm::vec4(color,1);data.parameter={emission,k1,k2,0};
}
directional_light::directional_light(object* value):normal_light(value,component_type::directional_light_type){}
glm::vec3 directional_light::get_direction() const{return world_direction(direction);}
void directional_light::apply_config(const light_config& c){normal_light::apply_config(c);direction=glm::make_vec3(c.direction.data());}
light_config directional_light::get_config() const{auto c=normal_light::get_config();c.type=light_type::directional;c.direction=array(direction);return c;}
void directional_light::update() noexcept{
    auto& data=egn.get_render_system().get_light_data().light_datas[idx];
    data.position=glm::vec4(0);data.direction=glm::vec4(get_direction(),0);
    data.color=glm::vec4(color,1);data.parameter={emission,0,0,0};
}
}
