# behavior

behavior 继承 component，作为 object 上可扩展的行为基类。object 可以拥有多个行为，负责统一更新和销毁。基类构造时保存 protected 的 object* const owner，behavior 不再缓存 transform、mesh、material 指针。派生类直接通过 owner 获取组件，也可使用 get_transform/get_mesh/get_material。业务更新逻辑继承 component::update，保持 protected 或 private。

组件 getter 直接返回 object 当前保存的指针，未挂载时为 nullptr；组件增加、删除和重新创建都不需要通知 behavior。object::update 在行为更新后更新 transform 的 Morton 编码，随后 late_update 更新空间索引。

通过 object::add_component<T>(...) 创建自定义行为；构造函数可保持私有，并声明 object 为友元。参数转发不包含 owner，object 自动传入自身。

```cpp
#include <core/object/object.hpp>

class spin_behavior final:public render::behavior {
    friend class render::object;
    float step;
    spin_behavior(render::object* owner,float radians_per_update):
        behavior(owner),step(radians_per_update){}
    void update() override {
        owner->get_transform()->rotation.y+=step;
        // owner->get_material()->get_sub_material(slot)->uniform_data 可修改材质参数。
        // owner->get_mesh()->get_data()/get_bounds()/get_cur_lod() 可读取几何及流式状态。
    }
};

// object 自带 transform；mesh/material 按对象需要添加。
render::object obj;
auto* spin=obj.add_component<spin_behavior>(0.01f);
// obj.remove_component(spin); // 移除单个行为；obj 析构也会统一释放。
```

行为只引用所属 object，不自行删除 object 或它的组件，也不拥有 GPU 资源。mesh 的流式加载、上传等内部接口仍由渲染系统管理。
