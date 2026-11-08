#ifndef _RENDER_GEOMETRY_HPP
#define _RENDER_GEOMETRY_HPP
#include <iostream>
#include <type_traits>
#include <limits>
#include <stdexcept>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

namespace render{
    struct triangle final{
        private:
        glm::vec3 e1{}, e2{};//边向量
        glm::vec3 centroid{};//重心
        glm::vec3 normal{0.f, 0.f, 1.f};//法线
        public:
        glm::vec3 x{}, y{}, z{};//三角形坐标
        triangle() = default;
        explicit triangle(const glm::vec3 _x,const glm::vec3 _y,const glm::vec3 _z):x(_x),y(_y),z(_z){
            if(!triangle_check(x,y,z)){
                throw std::invalid_argument("invaild triangle");
            }
            e1 = y-x,e2 = z-x;
            centroid = (x + y + z) / 3.0f;
            normal = normalize(cross(e1, e2));
        }

        inline static bool triangle_check(const glm::vec3 x,const glm::vec3 y,const glm::vec3 z) noexcept{
            glm::vec3 temp = glm::cross(y - x, z - x);
            if(glm::dot(temp,temp)<=1e-6f) return false;
            return true;
        }
        //对片元进行M变换
        inline void M(glm::mat4 m) noexcept{
            x = glm::vec3(m * glm::vec4(x, 1.0f));
            y = glm::vec3(m * glm::vec4(y, 1.0f));
            z = glm::vec3(m * glm::vec4(z, 1.0f));
            glm::mat4 normal_m = glm::transpose(glm::inverse(m));
        }

        inline void change(const glm::vec3 _x,const glm::vec3 _y,const glm::vec3 _z){
            if(!triangle_check(_x,_y,_z)){
                throw std::invalid_argument("invaild triangle");
            }
            x = _x,y = _y,z = _z;
            e1 = y-x,e2 = z-x;
            centroid = (x + y + z) / 3.0f;
            normal = normalize(cross(e1, e2));
        }

        inline glm::vec3 get_normal(){return normal;}
    };

    struct sphere final{
        glm::vec3 position;
        float radius;
        sphere(const glm::vec3& _postion,const float& _radius):position(_postion),radius(_radius){};
    };
    //顶点索引
    struct tri_idx final{
        uint32_t a, b, c;
        tri_idx(uint32_t _a,uint32_t _b,uint32_t _c):a(_a),b(_b),c(_c){}
    };

    struct Plane {
        glm::vec3 normal;
        float d;
        // 点到平面的距离
        inline float distance(const glm::vec3& point) const {
            return glm::dot(normal, point) + d;
        }
    };
}

#endif