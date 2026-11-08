#ifndef __RENDER_RENDER_HPP
#define __RENDER_RENDER_HPP
#include <vulkan/vulkan.h>
#include <core/render/render_resources.hpp>
#include <core/render/render_scene.hpp>
#include <core/render/data/prim_culling_data.hpp>
#include <file/config/scence_prim.hpp>
#include <file/config/resource/page_manager.hpp>
#include <vector>
#include <core/render/render_window.hpp>
///set 0 camera
///set 1 light
///set 2 material
///set 3 g_buffer
///set 4 OIT capture
///set 5 skybox atmosphere

namespace render{
    class normal_light;
    class main_light;
    class spirv_data;
    class render_system final{
        friend class normal_light;
        friend class main_light;
        friend class spirv_data;
    public:
        // init 创建支持 dynamicRendering、pixel interlock 和 R64_UINT storage image 的 Vulkan 1.3 设备。
        // 内置 shader 从传入目录读取，模块在管线创建后释放。
        render_system() = default;
        void init(const std::filesystem::path& shader_root,const render_window_info& window={});
        render_context get_context() const noexcept;
        render_image get_color_image() const noexcept;
        void resize_viewport(VkExtent2D extent);
        void shutdown() noexcept;
        ~render_system();

        render_system(const render_system&) = delete;
        render_system& operator=(const render_system&) = delete;
        render_system(render_system&&) = delete;
        render_system& operator=(render_system&&) = delete;

        const render_resources& get_resources() const noexcept { return res; }
        render_resources& get_resources() noexcept { return res; }
        render_scene& get_scene() noexcept { return scene; }
        const render_scene& get_scene() const noexcept { return scene; }
        scence_prim& get_prim_table() noexcept { return prim_table; }
        page_manager& get_page_manager() noexcept { return prim_cache; }
        light_data& get_light_data() noexcept {return res.light;}
        skybox_data& get_skybox_data() noexcept {return res.sky;}
        //渲染流程
        void pass();
    private:
        std::filesystem::path shader_root;
        render_context context;
        render_resources res;///渲染资源
        render_scene scene;
        scence_prim prim_table;
        page_manager prim_cache;
        uint64_t stream_frame = 0;
        prim_culling_data culling;
        VkPipeline cull_pipeline = VK_NULL_HANDLE;
        bool culling_enabled=true;
        VkPipelineLayout cull_pipeline_layout = VK_NULL_HANDLE;
        struct cull_draw { object* obj; sub_mesh sub; uint32_t command_index; };
        std::vector<cull_draw> opaque_draws, transparent_draws;
        VkPipeline geometry_pipeline = VK_NULL_HANDLE;
        VkPipeline skybox_pipeline=VK_NULL_HANDLE;
        VkPipeline tone_mapping_pipeline=VK_NULL_HANDLE;
        bool skybox_enabled=true,tone_mapping_enabled=true;
        VkPipeline transparent_pipeline = VK_NULL_HANDLE;
        VkPipeline oit_resolve_pipeline = VK_NULL_HANDLE;
        VkPipeline oit_sort_pipeline = VK_NULL_HANDLE;
        VkPipelineLayout oit_pipeline_layout = VK_NULL_HANDLE;
        VkPipelineLayout geometry_pipeline_layout = VK_NULL_HANDLE;
        uint32_t patch_control_points = 3;//细分管线的控制点数量

        bool attachments_initialized = false;

        VkInstance instance = VK_NULL_HANDLE;
        VkPhysicalDevice physical_device = VK_NULL_HANDLE;
        VkDevice device = VK_NULL_HANDLE;
        void init_dev(const render_window_info& window);
        void init_pl();//初始化管线
        enum class pipeline_pass{opaque, transparent_capture, transparent_resolve, skybox, tone_mapping};
        VkPipeline create_pipeline(const std::vector<VkPipelineShaderStageCreateInfo>& stages,
                                   pipeline_pass pass = pipeline_pass::opaque);
        void geometry_pass(VkCommandBuffer command_buffer);
        void post_processing_pass(VkCommandBuffer command_buffer);
        void prepare_culling();
        void pre_processing_pass(VkCommandBuffer command_buffer);
        void opaque_rendering(VkCommandBuffer command_buffer);
        void transparent_rendering(VkCommandBuffer command_buffer);
        void transition_attachments(VkCommandBuffer command_buffer, bool for_rendering);
        void cleanup() noexcept;
    };
} 

#endif

