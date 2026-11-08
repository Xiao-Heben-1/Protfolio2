#ifndef __RENDER_TRANSFORM_HPP
#define __RENDER_TRANSFORM_HPP

#include <core/component/component.hpp>
#include <glm/glm.hpp>
namespace render{
class object;
class transform final:public component{
    friend class object;
public:
    glm::vec3 position{0.0f};
    glm::vec3 rotation{0.0f};//欧拉角，单位为弧度
    glm::vec3 scale{1.0f};
protected:
    uint64_t morton_code;///莫顿编码
public:
    glm::mat4 get_matrix() const noexcept;
protected:
    void upload(VkCommandBuffer command_buffer,VkPipelineLayout pipeline_layout) override;
    void update() override;
    explicit transform(object* owner);
    ~transform() override = default;
};
}

#endif
