#ifndef __RENDER_PRIMITIVE_HPP
#define __RENDER_PRIMITIVE_HPP


#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <tools/tools.hpp>
#include <tools/structures/AABB.hpp>
namespace render{
    //基础单元
    struct primitive final{
        private:
        int id = -1;//在Primitive列表的索引
        int node = -1;//在BVHNode列表的索引
        int treelet = -1;//在哪棵子树
        int treeletId = -1;
        uint32_t object_id;//对应的object_id
        int meshId = -1;//mesh当中的primiviate索引
        AABB bbox{};
        ::render::triangle t{};
        public:
        primitive() = default;
        explicit primitive(const ::render::triangle& _t,int _id,uint32_t _object_id,int _meshId):t(_t),id(_id),object_id(_object_id),meshId(_meshId){
            bbox.min = glm::min(t.x,glm::min(t.y,t.z));
            bbox.max = glm::max(t.x,glm::max(t.y,t.z));
        } 
        ~primitive() {} 
        friend struct BVHNode;
        friend struct BVH;
    };

}

#endif


