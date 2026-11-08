#ifndef __RENDER_CORE_OBJECT_HPP
#define __RENDER_CORE_OBJECT_HPP
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <core/component/component_enter.hpp>
#include <vector>
#include <utility>
namespace render{
    class object_manger;
    class object_loader;
    class render_system;
    class object{
        friend class component;
        friend class mesh;
        friend class material;
        friend class transform;
        friend struct camera;
        friend class object_manger;
        friend class object_loader;
        friend class render_system;
    public:
        object();
        ~object();
        object(const object&) = delete;
        object& operator=(const object&) = delete;
        component* add_component(int type);
        template<class T,class... Args> T* add_component(Args&&... args){
            return new T(this,std::forward<Args>(args)...);
        }
        void remove_component(component* comp) noexcept;
        void set_transparent(bool value);
        transform* get_transform() noexcept { return tran; }
        const transform* get_transform() const noexcept { return tran; }
        ::render::mesh* get_mesh() noexcept { return mesh; }
        const ::render::mesh* get_mesh() const noexcept { return mesh; }
        material* get_material() noexcept { return mat; }
        const material* get_material() const noexcept { return mat; }
        camera* get_camera() noexcept { return cam; }
        const camera* get_camera() const noexcept { return cam; }
        bool get_transparent() const noexcept{return is_transparent;}
        const std::vector<component*>& get_components() const noexcept{return components;}
        void update();///每帧先更新组件
        void late_update();///更新Morton空间页表及三档资源引用
    private:
        bool is_transparent = false;//只能通过 set_transparent 切换分类列表
        uint32_t run_id;
        uint32_t object_idx;//在 objects 总列表中的索引
        uint32_t idx;//两个分类 vector 共用的位置索引，由 is_transparent 确定所属列表
        std::vector<component*> components;//组件列表
        material* mat = nullptr;
        mesh* mesh = nullptr;
        transform* tran = nullptr;
        camera* cam = nullptr;
        void upload(VkCommandBuffer command_buffer,VkPipelineLayout pipeline_layout);///将组件数据绑定到当前渲染指令
        component* create_component(int type);
    };

}
#endif
