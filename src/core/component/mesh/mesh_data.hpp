#pragma once
#include <cstdint>
#include <vector>
#include <glm/glm.hpp>
namespace render {
struct vertex final {
    glm::vec3 position;
    glm::vec2 uv;
    glm::vec3 normal;
    vertex() = default;
    vertex(const glm::vec3& p,const glm::vec2& t,const glm::vec3& n):position(p),uv(t),normal(glm::normalize(n)){}
};
struct sub_mesh final {
    uint32_t front_idx = 0; 
    uint32_t index_count = 0;
    int32_t vertex_off = 0;
    uint32_t mat_idx = 0;
};
struct mesh_data final {
    std::vector<vertex> vertices;
    std::vector<uint32_t> indices;
    std::vector<sub_mesh> sub_meshes;
};
}
