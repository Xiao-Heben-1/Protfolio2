#ifndef __RENDER_BVH_HPP
#define __RENDER_BVH_HPP
#include <tools/structures/BVH/BVH_node.hpp>
#include <tools/structures/BVH/primitive.hpp>
#include <unordered_map>
#include <queue>
namespace render{
    class BVH{
        public:

        private:
        //初始化建树
        void build();
        //计算SAH
        int SAH();
        //
        void rebuild();
        //局部旋转
        void change();
        uint32_t root;
    };
    class LBVH:public BVH{
        public:
        //插入新片元
        void add_prim();
        //删除片元
        void erase_prim(const std::vector<int>& prims);
        private:
        std::vector<cluster_BVH_node> node_lists;
    };
    class SBVH:public BVH{
        private:
        struct object_record { AABB bounds; int node=-1; size_t offset=0; };
        struct temp_node { AABB bounds; uint32_t object; };
        std::vector<space_BVH_node> node_lists;
        std::unordered_map<uint32_t,object_record> objects;
        std::queue<int> node_hollow;
        int root_index=-1;
        bool rebuild_pending=false;
        public:
        void upsert(uint32_t object_id,const AABB& bounds);
        void erase_node(uint32_t object_id);
        std::vector<uint32_t> query(const AABB& bounds);
        private:
        int allocate_node(const AABB& bounds,int parent);
        void release_node(int node);
        void build();
        int SAH(std::vector<temp_node>& entries,const AABB& bounds,int start,int end,int dim);
        int upperSAH(std::vector<temp_node>& entries,int parent,int start,int end);
        void SAHInsert(uint32_t object_id);
        void SAHUpdate(int node);
        int SAHRebuild(int node);
        void update(int node);
    };
}

#endif
