#ifndef __RENDER_AABB_HPP
#define __RENDER_AABB_HPP

#include <limits>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
namespace render{
    //包围盒
    struct AABB final{
        private:
        glm::vec3 min;//最小点
        glm::vec3 max;//最大点
        inline bool operator<=(const AABB& b)const noexcept{
            return min.x >= b.min.x && min.y >= b.min.y && min.z >= b.min.z &&
                max.x <= b.max.x && max.y <= b.max.y && max.z <= b.max.z;
            
        }

        //合并包围盒
        inline friend AABB merge(const AABB& a,const AABB& b){
            return AABB(glm::min(a.min,b.min),glm::max(a.max,b.max));
        } 
        //计算包围盒面积
        inline float SA()const noexcept{
            glm::vec3 d = max - min;
            if (d.x < 0.0f || d.y < 0.0f || d.z < 0.0f)
                return 0.0f;
            return 2.0f * (d.x * d.y + d.y * d.z + d.z * d.x);
        }
        //获得包围盒中心点
        inline glm::vec3 center()const noexcept{
            return (min + max) * 0.5f;
        }
        //获得包围盒当前维度
        inline const int getDim()const noexcept{
            glm::vec3 diag = max - min;
            return (diag.x > diag.y && diag.x > diag.z) ? 0 :(diag.y > diag.z ? 1 : 2);
        }
        public:
        const glm::vec3& get_min() const noexcept { return min; }
        const glm::vec3& get_max() const noexcept { return max; }
        glm::vec3 get_center() const noexcept { return center(); }
        bool intersects(const AABB& other) const noexcept {
            return glm::all(glm::lessThanEqual(min,other.max)) && glm::all(glm::greaterThanEqual(max,other.min));
        }
        bool operator==(const AABB& other) const noexcept { return min==other.min && max==other.max; }
        AABB transformed(const glm::mat4& matrix) const noexcept {
            const auto c=glm::vec3(matrix*glm::vec4(center(),1));
            const auto e=(max-min)*0.5f;
            const auto r=glm::abs(glm::vec3(matrix[0]))*e.x+glm::abs(glm::vec3(matrix[1]))*e.y+glm::abs(glm::vec3(matrix[2]))*e.z;
            return AABB(c-r,c+r);
        }
        AABB(): min(std::numeric_limits<float>::max()),max(std::numeric_limits<float>::lowest()) {}
        explicit AABB(const glm::vec3& p): min(p), max(p) {}
        explicit AABB(const glm::vec3& p1, const glm::vec3& p2): min(glm::min(p1, p2)),max(glm::max(p1, p2)) {}

        friend struct primitive;
        friend struct morton;
        friend struct BVH_node;
        friend struct cluster_BVH_node;
        friend struct space_BVH_node;
        friend struct octree_node;
        friend class BVH;
        friend class SBVH;
        friend class octree;
    };
}
#endif
