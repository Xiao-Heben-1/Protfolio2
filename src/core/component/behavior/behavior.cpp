#include <core/component/behavior/behavior.hpp>
#include <core/object/object.hpp>
namespace render {
behavior::behavior(object* owner):component(owner,static_cast<uint32_t>(component_type::behavior_type)){}
behavior::~behavior() = default;
::render::mesh* behavior::get_mesh() noexcept{return owner->get_mesh();}
const ::render::mesh* behavior::get_mesh() const noexcept{return owner->get_mesh();}
material* behavior::get_material() noexcept{return owner->get_material();}
const material* behavior::get_material() const noexcept{return owner->get_material();}
}
