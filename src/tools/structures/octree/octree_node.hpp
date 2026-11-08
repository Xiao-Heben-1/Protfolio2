#ifndef OCTREE_NODE_HPP
#define OCTREE_NODE_HPP

// #include <array>
// #include <cstdint>
// #include <vector>

// #include <tools/structures/AABB.hpp>

// namespace render{

// class octree;

// struct octree_node final{
// private:
//     AABB bbox;//包围盒
//     AABB max_bbox;//最大允许扩容包围盒
//     std::array<int, 8> children;//八个子节点
//     int parent;//父节点
//     int depth;//节点深度
//     std::vector<uint32_t> object_ids;//当前节点挂载的object

//     explicit octree_node(AABB bbox = {}, int parent = -1, int depth = 0):bbox(std::move(bbox)), parent(parent), depth(depth){
//         children.fill(-1);
//         glm::vec3 center = (bbox.min + bbox.max) * 0.5f;
//         glm::vec3 half_size = (bbox.max - bbox.min) * 0.75f;
//         max_bbox = AABB(center - half_size, center + half_size);
//     }

//     octree_node(const octree_node&) = default;
//     octree_node(octree_node&&) noexcept = default;
//     octree_node& operator=(const octree_node&) = default;
//     octree_node& operator=(octree_node&&) noexcept = default;

//     inline bool is_leaf() const noexcept{return children[0] == -1;}

//     friend class octree;
// };

// }

#endif // OCTREE_NODE_HPP