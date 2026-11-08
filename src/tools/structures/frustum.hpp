#pragma once
#include <tools/structures/AABB.hpp>
#include <array>
namespace render {
struct frustum {
    std::array<glm::vec4,6> planes{};
    AABB bounds;
    explicit frustum(const glm::mat4& vp=glm::mat4(1));
    bool intersects(const AABB& box) const noexcept;
};
}
