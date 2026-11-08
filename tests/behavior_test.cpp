#include <core/object/object.hpp>
#include <cassert>
#include <iostream>
#include <engine.hpp>

class move_behavior final:public render::behavior {
    friend class render::object;
    glm::vec3 step;
    int& destroyed;
    int updates=0;
    render::mesh* observed_mesh=nullptr;
    render::material* observed_material=nullptr;
    move_behavior(render::object* owner,glm::vec3 delta,int& destruction_count):
        behavior(owner),step(delta),destroyed(destruction_count){}
    ~move_behavior() override{++destroyed;}
    void update() override{
        owner->get_transform()->position+=step;
        observed_mesh=owner->get_mesh();
        observed_material=owner->get_material();
        ++updates;
    }
public:
    int get_updates() const{return updates;}
    render::mesh* get_observed_mesh() const{return observed_mesh;}
    render::material* get_observed_material() const{return observed_material;}
};

int main(){
    int destroyed=0;
    {
        render::object object;
        auto* first=object.add_component<move_behavior>(glm::vec3(1,0,0),destroyed);
        assert(first->get_transform()==object.get_transform());
        assert(!first->get_mesh() && !first->get_material());
        object.update();
        assert(first->get_updates()==1 && object.get_transform()->position==glm::vec3(1,0,0));

        auto* geometry=static_cast<render::mesh*>(object.add_component(int(render::component_type::mesh_type)));
        auto* material=object.add_component<render::material>();
        assert(first->get_mesh()==geometry && first->get_material()==material);
        auto* second=object.add_component<move_behavior>(glm::vec3(0,2,0),destroyed);
        const auto& readonly=*second;
        assert(readonly.get_transform()==object.get_transform() && readonly.get_mesh()==geometry && readonly.get_material()==material);
        object.update();
        assert(first->get_updates()==2 && second->get_updates()==1);
        assert(object.get_transform()->position==glm::vec3(2,2,0));
        assert(first->get_observed_mesh()==geometry && second->get_observed_material()==material);

        // 非尾部组件删除会交换注册表索引；行为从 object 读取当前组件。
        object.remove_component(geometry);
        assert(!first->get_mesh() && !second->get_mesh());
        geometry=static_cast<render::mesh*>(object.add_component(int(render::component_type::mesh_type)));
        assert(first->get_mesh()==geometry && second->get_mesh()==geometry);
        object.remove_component(material);
        assert(!first->get_material() && !second->get_material());
        material=object.add_component<render::material>();
        assert(first->get_material()==material && second->get_material()==material);

        object.remove_component(object.get_transform());
        assert(!first->get_transform() && !second->get_transform());
        auto* transform=object.add_component<render::transform>();
        assert(first->get_transform()==transform && second->get_transform()==transform);
        object.update();
        assert(first->get_updates()==3 && second->get_updates()==2);
        assert(transform->position==glm::vec3(1,2,0));

        auto* base=static_cast<render::behavior*>(object.add_component(int(render::component_type::behavior_type)));
        assert(base->get_transform()==transform && base->get_mesh()==geometry && base->get_material()==material);
        object.remove_component(first);
        assert(destroyed==1);
        object.update();
        assert(second->get_updates()==3);
        {
            render::object other;
            auto* script=other.add_component<move_behavior>(glm::vec3(0,0,3),destroyed);
            assert(script->get_transform()==other.get_transform() && !script->get_mesh() && !script->get_material());
            other.update();
            assert(other.get_transform()->position==glm::vec3(0,0,3));
            assert(transform->position==glm::vec3(1,4,0));
        }
        assert(destroyed==2);
    }
    assert(destroyed==3);
    // 主相机的内置行为消费一次输入；移动、平移、缩放和聚焦不依赖窗口。
    auto& scene=render::egn.get_render_system().get_scene();
    auto& camera=scene.get_main_camera();
    auto pose=camera.get_config();pose.position={0,0,0};pose.direction={0,0,-1};
    auto& control=camera.get_behavior();control.get_input().pose=pose;scene.update();
    control.get_input().move={0,0,1};control.get_input().delta_time=1;scene.update();
    assert(glm::length(camera.get_position()-glm::vec3(0,0,-10))<.001f);
    scene.update();assert(glm::length(camera.get_position()-glm::vec3(0,0,-10))<.001f);
    control.get_navigation().pan_speed=.02f;control.get_navigation().zoom_speed=2;
    control.get_input().pan={10,0};control.get_input().zoom=2;scene.update();
    assert(glm::length(camera.get_position()-glm::vec3(-.2f,0,-14))<.001f);
    control.get_input().look={100,-100};control.get_input().focus=glm::vec3(2,3,4);control.get_input().focus_distance=12;scene.update();
    assert(std::abs(glm::length(camera.get_position()-glm::vec3(2,3,4))-12)<.001f);
    const auto clip=camera.get_vp()*glm::vec4(2,3,4,1);
    assert(std::abs(clip.x/clip.w)<.001f && std::abs(clip.y/clip.w)<.001f);
    // Camera 组件共享唯一渲染入口；切换、Transform 和销毁回退。
    {
        render::object first,second;
        auto* a=first.add_component<render::camera>();auto* b=second.add_component<render::camera>();
        assert(!a->get_is_main() && !b->get_is_main());
        first.get_transform()->position={1,2,3};second.get_transform()->position={10,20,30};
        a->set_main();scene.update();assert(&scene.get_main_camera()==a && a->get_is_main() && !camera.get_is_main());
        assert(a->get_position()==glm::vec3(1,2,3));
        b->set_main();scene.update();assert(&scene.get_main_camera()==b && !a->get_is_main() && b->get_is_main());
        auto& input=b->get_behavior().get_input();input.look={200,0};input.move={0,0,1};input.delta_time=.1f;
        scene.update();assert(b->get_config().direction[0]>0); // 向右拖拽向右转。
        const auto direction=b->get_config().direction;scene.update();
        assert(std::abs(b->get_config().direction[0]-direction[0])<.0001f);
        assert(second.get_transform()->position==b->get_position());
        second.remove_component(b);assert(&scene.get_main_camera()==&camera && camera.get_is_main());
        a->set_main();first.remove_component(a);assert(&scene.get_main_camera()==&camera);
    }
    {
        render::object sun_object;auto* sun=sun_object.add_component<render::main_light>();sun_object.update();
        auto& gpu=render::egn.get_render_system().get_light_data().main_light_data;
        const auto direction=gpu.direction;
        sun_object.get_transform()->position={1000,2000,-3000};sun_object.update();
        assert(gpu.direction==direction && glm::vec3(gpu.solar_irradiance)==sun->get_solar_irradiance());
        sun_object.get_transform()->rotation.z=1;sun_object.update();assert(gpu.direction!=direction);
        {
            render::object second;second.add_component<render::main_light>();second.update();
            assert(gpu.direction==direction);
        }
        assert(gpu.direction!=direction); // 删除当前太阳后立即恢复上一太阳。
    }
    assert(glm::vec3(render::egn.get_render_system().get_light_data().main_light_data.solar_irradiance)==glm::vec3(0));
    std::cout<<"Behavior, component ownership, main camera and Sun direction lifecycle passed\n";
}
