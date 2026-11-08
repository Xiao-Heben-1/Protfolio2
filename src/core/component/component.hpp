#ifndef __RENDER_COMPONENT_HPP
#define __RENDER_COMPONENT_HPP

#include <vulkan/vulkan.h>
namespace render{
    enum class component_type:uint32_t{
        transform_type = 1,
        mesh_type = 2,
        material_type = 3,
        behavior_type = 4,
        camera_type = 5,
        main_light_type = 6,
        point_light_type = 7,
        directional_light_type = 8,
    };

    class object;
    class transform;
    class render;
    class component{
        friend class object;
        friend class render;
        protected:
            object* const owner;
        public:
            uint32_t get_type() const noexcept{return type;}
            transform* get_transform() noexcept;
            const transform* get_transform() const noexcept;
        protected:
            component(object* _owner,uint32_t _type);
            virtual ~component();
            component(const component&) = delete;
            component& operator=(const component&) = delete;
            component(component&&) = delete;
            component& operator=(component&&) = delete;
            virtual void init(VkDevice _device) noexcept{device = _device;}///初始化逻辑设备
            virtual void upload(VkCommandBuffer command_buffer,VkPipelineLayout pipeline_layout){};///将组件数据绑定到当前渲染指令
            virtual void update() {};///更新数据
            virtual void cleanup() {};///清除数据
            VkDevice device = VK_NULL_HANDLE;
        private:
            const uint32_t type = 0;///组件类型
            int idx = -1;//未注册时为 -1
    };
}
#endif // __RENDER_COMPONENT_HPP
