#pragma once
#include <core/component/component.hpp>
#include <file/config/component/light_config.hpp>
#include <glm/glm.hpp>
namespace render {
namespace front {class editor;}
class light_manger;
class light:public component {
    friend class object_loader;
    friend class front::editor;
protected:
    std::string guid=generate_guid(),name;
    light(object* owner,component_type type);
    ~light() override=default;
    virtual void apply_config(const light_config& config);
    glm::vec3 world_direction(glm::vec3 local) const;
public:
    virtual light_config get_config() const;
};
// 唯一生效的太阳；不保存点光源位置、距离衰减参数。
class main_light final:public light {
    friend class object;
    friend class light_manger;
private:
    glm::vec3 direction{-.4f,-1,-.5f}; // 光传播方向；GPU 中取反得到指向太阳的方向。
    glm::vec3 solar_irradiance{20}; // 大气层外法向辐照度，线性 RGB，W/m²。
    float angular_radius=.004675f; // 可见太阳角半径，rad。
    float disk_intensity=1; // 只调节可见圆盘亮度，不改变物体直射光。
    explicit main_light(object* owner);
    ~main_light() override;
    void apply_config(const light_config& config) override;
    void update() noexcept override;
public:
    glm::vec3 get_direction() const;
    glm::vec3 get_solar_irradiance() const;
    float get_angular_radius() const;
    float get_disk_intensity() const;
    light_config get_config() const override;
};
class normal_light:public light {
    friend class light_manger;
protected:
    glm::vec3 color{1};
    float emission=1;
    int idx=-1;
    normal_light(object* owner,component_type type);
    ~normal_light() override;
    void apply_config(const light_config& config) override;
public:
    glm::vec3 get_color() const;
    float get_emission() const;
    light_config get_config() const override;
};
class point_light final:public normal_light {
    friend class object;
private:
    glm::vec3 position{};
    float k1=.7f,k2=1.8f;
    explicit point_light(object* owner);
    void apply_config(const light_config& config) override;
    void update() noexcept override;
public:
    glm::vec3 get_position() const;
    float get_k1() const;
    float get_k2() const;
    light_config get_config() const override;
};
class directional_light final:public normal_light {
    friend class object;
private:
    glm::vec3 direction{0,-1,0};
    explicit directional_light(object* owner);
    void apply_config(const light_config& config) override;
    void update() noexcept override;
public:
    glm::vec3 get_direction() const;
    light_config get_config() const override;
};
}
