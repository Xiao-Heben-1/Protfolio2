#pragma once
#include <core/component/component.hpp>
namespace render {
class mesh;
class material;
// 复用 component 构造时保存的 owner，访问组件时直接从 object 获取。
class behavior:public component {
    friend class object;
public:
    ::render::mesh* get_mesh() noexcept;
    const ::render::mesh* get_mesh() const noexcept;
    material* get_material() noexcept;
    const material* get_material() const noexcept;
protected:
    explicit behavior(object* owner);
    ~behavior() override;
    // 派生类覆盖 component::update()；object 在所有行为更新后更新 transform。
};
}
