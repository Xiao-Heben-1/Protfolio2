#ifndef __RENDER_OBJECT_MANAGER_HPP
#define __RENDER_OBJECT_MANAGER_HPP
#include <core/object/object.hpp>
#include <vector>
#include <tools/math/random.hpp>
namespace render {
class render_system;
class object_manger final{
    friend class object;
    friend class render_system;
    friend class render_scene;
private:
    UUIDGenerator gen;//随机uid生成器
    object_manger();
    object_manger(const object_manger& other);
    object_manger& operator=(const object_manger& other);

    std::vector<object*> objects;
    std::vector<object*> opaque_objects;//非透明物体
    std::vector<object*> transparent_objects;//透明物体
};

}
#endif // __RENDER_OBJECT_MANAGER_HPP
