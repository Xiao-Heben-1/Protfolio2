#ifndef __RENDER_CAMERA_HPP
#define __RENDER_CAMERA_HPP
#include <core/component/component.hpp>
#include <tools/structures/frustum.hpp>
#include <array>
#include <core/component/behavior/camera_behavior.hpp>
namespace render {
class render_scene;
struct camera final:public component {
    friend class render_system;
    friend class render_resources;
    friend class render_scene;
    friend class mesh_stream_test;
    friend class camera_behavior;
    friend class object;
    friend class object_loader;
private:
    render_scene* scene=nullptr;
    bool is_main=false;
    glm::vec3 position;
    glm::vec3 direction;
    float fov;
    float aspect;
    float near_plane;
    float far_plane;
    glm::mat4 view{1};
    glm::mat4 projection{1};
    glm::mat4 vp{1};
    frustum view_frustum;
    VkExtent2D extent{1280,720};
    VkFormat g_buffer_format=VK_FORMAT_R16G16B16A16_SFLOAT;
    VkFormat depth_format=VK_FORMAT_D32_SFLOAT;
    camera_behavior navigation;
public:
    ~camera() override;
    bool get_is_main() const noexcept{return is_main;}
    void set_main();
    // 四段宽度，可在运行时修改：三档显示范围 + 加载缓冲区。
    inline static std::array<float,4> dis{128.f,512.f,512.f,1024.f};
    const glm::vec3& get_position() const noexcept;
    camera_behavior& get_behavior() noexcept{return navigation;}
    camera_config get_config() const;
    const glm::mat4& get_view() const noexcept;
    const glm::mat4& get_projection() const noexcept;
    const glm::mat4& get_vp() const noexcept;
    const frustum& get_frustum() const noexcept;
private:
    explicit camera(object* owner);
    camera(float aspect=16.f/9,glm::vec3 position=glm::vec3(0),glm::vec3 direction=glm::vec3(0,0,-1),float fov=45,float near_plane=.1f,float far_plane=1152.f);
    camera& operator=(const camera& other) noexcept;
    void update_view() noexcept;
    void update_projection() noexcept;
    void set_position(const glm::vec3& value) noexcept;
    void set_direction(const glm::vec3& value) noexcept;
    void apply_config(const camera_config& config);
protected:
    void update() noexcept override;
};
}
#endif
