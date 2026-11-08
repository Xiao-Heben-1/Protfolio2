#include <core/component/component.hpp>
#include <core/object/object.hpp>
#include <stdexcept>

namespace render{
transform* component::get_transform() noexcept{
    return owner->tran;
}

const transform* component::get_transform() const noexcept{
    return owner->tran;
}

component::component(object* _owner,uint32_t _type):owner(_owner),type(_type){
    if(owner == nullptr) return; // 独立主相机不注册到object。
    const auto kind = static_cast<component_type>(type);
    if((kind == component_type::mesh_type && owner->mesh != nullptr) || (kind == component_type::material_type && owner->mat != nullptr) || (kind == component_type::transform_type && owner->tran != nullptr) || (kind == component_type::camera_type && owner->cam != nullptr)){
        throw std::logic_error("Object already owns a component of this type");
    }
    owner->components.push_back(this);
    idx = static_cast<int>(owner->components.size() - 1);
}

component::~component(){
    if(owner == nullptr || idx < 0) return;
    if(owner->mesh == this) owner->mesh = nullptr;
    if(owner->mat == this) owner->mat = nullptr;
    if(owner->tran == this) owner->tran = nullptr;
    if(owner->cam == this) owner->cam = nullptr;
    auto& components = owner->components;
    components[idx] = components.back();
    components[idx]->idx = idx;
    components.pop_back();
}
}
