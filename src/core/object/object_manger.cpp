#include <core/object/object_manger.hpp>

namespace render{
object_manger::object_manger(){
    objects.reserve(100);
    opaque_objects.reserve(100);
    transparent_objects.reserve(100);
}

object_manger::object_manger(const object_manger& other){
    *this = other;
}

object_manger& object_manger::operator=(const object_manger& other){
    if(this == &other) return *this;
    objects = other.objects;
    opaque_objects = other.opaque_objects;
    transparent_objects = other.transparent_objects;
    for(uint32_t i = 0; i < objects.size(); ++i) objects[i]->object_idx = i;
    for(uint32_t i = 0; i < opaque_objects.size(); ++i) opaque_objects[i]->idx = i;
    for(uint32_t i = 0; i < transparent_objects.size(); ++i) transparent_objects[i]->idx = i;
    return *this;
}
}
