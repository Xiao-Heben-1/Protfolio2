#ifndef __RENDER_BVH_NODE_HPP
#define __RENDER_BVH_NODE_HPP
#include <tools/structures/BVH/primitive.hpp>

namespace render{

/// @brief BVH节点基类
struct BVH_node{
    friend class SBVH;
protected:
    AABB bbox;//包围盒
    int lc;//左孩子
    int rc;//右孩子
    int fa;//父亲节点
    int bit_index;
    uint32_t bit;
    bool dirty = false;

    explicit BVH_node(AABB bbox = {}, int bit_index = 18):bbox(std::move(bbox)), bit_index(bit_index){
        lc = rc = fa = -1;
    }

    inline bool is_leaf() const noexcept{return lc == -1;}

public:
    BVH_node(const BVH_node&) = default;
    BVH_node(BVH_node&&) noexcept = default;
    BVH_node& operator=(const BVH_node&) = default;
    BVH_node& operator=(BVH_node&&) noexcept = default;
};


/// @brief mesh内部cluster管理的BVH节点
struct cluster_BVH_node final : BVH_node{
private:
    std::vector<int> prim;//cluster片元
    int prim_size = 0;

    inline float cost(const std::vector<cluster_BVH_node>& BVH_nodes) const noexcept{
        return BVH_nodes[lc].bbox.SA() + BVH_nodes[rc].bbox.SA();
    }

public:
    explicit cluster_BVH_node(AABB bbox = {}, std::vector<int> prim = {}, int bit_index = 18):BVH_node(std::move(bbox), bit_index), prim(std::move(prim)){
        this->prim.reserve(20);
    }

    cluster_BVH_node(const cluster_BVH_node&) = default;
    cluster_BVH_node(cluster_BVH_node&&) noexcept = default;
    cluster_BVH_node& operator=(const cluster_BVH_node&) = default;
    cluster_BVH_node& operator=(cluster_BVH_node&&) noexcept = default;

    friend struct cluster_BVH;
};


/// @brief 全局空间object管理的BVH节点
struct space_BVH_node final : BVH_node{
    friend class SBVH;
private:
    std::vector<uint32_t> objects;//S层叶子直接引用对象，不创建下层LBVH。
    uint32_t count = 0;
    float sah = 0;//建树时面积，用于判断局部重建。

    inline float cost(const std::vector<space_BVH_node>& BVH_nodes) const noexcept{
        return BVH_nodes[lc].bbox.SA() + BVH_nodes[rc].bbox.SA();
    }

public:
    explicit space_BVH_node(AABB bbox = {}):BVH_node(std::move(bbox)){}

    space_BVH_node(const space_BVH_node&) = default;
    space_BVH_node(space_BVH_node&&) noexcept = default;
    space_BVH_node& operator=(const space_BVH_node&) = default;
    space_BVH_node& operator=(space_BVH_node&&) noexcept = default;

    friend struct space_BVH;
};

}

#endif
