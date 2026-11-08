#include <core/component/transform/transform.hpp>
#include <core/object/object.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <tools/structures/BVH/morton.hpp>
namespace render{
    transform::transform(object* owner):component(owner,1){
        morton_code = morton::position_to_morton64(position);
        if(owner != nullptr) owner->tran = this;
    }

    glm::mat4 transform::get_matrix() const noexcept{
        glm::mat4 matrix{1.0f};
        matrix = glm::translate(matrix, position);
        matrix = glm::rotate(matrix, rotation.x, glm::vec3{1.0f, 0.0f, 0.0f});
        matrix = glm::rotate(matrix, rotation.y, glm::vec3{0.0f, 1.0f, 0.0f});
        matrix = glm::rotate(matrix, rotation.z, glm::vec3{0.0f, 0.0f, 1.0f});
        matrix = glm::scale(matrix, scale);

        return matrix;
    }

    void transform::upload(VkCommandBuffer command_buffer,VkPipelineLayout pipeline_layout){
        glm::mat4 model = get_matrix();
        vkCmdPushConstants(command_buffer,pipeline_layout,VK_SHADER_STAGE_VERTEX_BIT,0,sizeof(glm::mat4),&model);
    }

    void transform::update(){
        morton_code = morton::position_to_morton64(position);
    }

}
