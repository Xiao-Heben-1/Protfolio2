#ifndef __RENDER_MESH_HPP
#define __RENDER_MESH_HPP
#include <core/component/component.hpp>
#include <core/component/mesh/mesh_data.hpp>
#include <file/config/resource/mesh_lod_resource.hpp>
#include <array>
namespace render {
class camera;
class mesh final:public component {
    friend class object;
    friend class object_loader;
    friend class render_system;
    friend class mesh_stream_test;
private:
    struct lod_gpu {
        VkBuffer vertices=VK_NULL_HANDLE,indices=VK_NULL_HANDLE;
        VkDeviceMemory vertex_memory=VK_NULL_HANDLE,index_memory=VK_NULL_HANDLE;
        std::vector<std::optional<page_handle>> pending;
        std::vector<uint8_t> uploaded;
        bool ready=false;
    };
    struct staging_page { VkBuffer buffer; VkDeviceMemory memory; };
    VkPhysicalDevice physical_device=VK_NULL_HANDLE;
    VkCommandPool command_pool=VK_NULL_HANDLE;
    VkQueue graphics_queue=VK_NULL_HANDLE;
    VkBuffer vertex_buffer=VK_NULL_HANDLE,index_buffer=VK_NULL_HANDLE;
    VkDeviceMemory vertex_memory=VK_NULL_HANDLE,index_memory=VK_NULL_HANDLE;
    std::array<mesh_lod_resource,3> lods;
    std::array<lod_gpu,3> gpu_lods;
    std::vector<staging_page> staging;
    std::string builtin_geometry;
    AABB bounds;
    mesh_data data; // 流式路径只保留当前子网格目录，顶点/索引按需从磁盘读
    bool streamed=false;
    bool drawable=false;
    int cur_lod=-1;
public:
    const mesh_data& get_data() const noexcept;
    const AABB& get_bounds() const noexcept;
    const std::array<mesh_lod_resource,3>& get_lods() const noexcept;
    int get_cur_lod() const noexcept;
    const std::string& get_guid() const noexcept;
    VkBuffer get_vertex_buffer() const noexcept;
    VkBuffer get_index_buffer() const noexcept;
private:
    mesh(object* owner,VkPhysicalDevice physical,VkDevice logical,VkCommandPool pool,VkQueue queue);
    ~mesh();
    void create(mesh_data&& source);
    void create_streamed(std::array<mesh_lod_resource,3> resources);
    void update_stream(const camera& cam,const AABB& world_bounds,page_manager& pages,uint64_t frame);
    void request_lod(uint32_t level,page_manager& pages,uint64_t frame);
    void stream_upload(VkCommandBuffer command,page_manager& pages);
    void finish_stream();
    void set_transparent(bool value);
    void release_lod(uint32_t level);
    void release_stream();
    uint32_t find_memory_type(uint32_t filter,VkMemoryPropertyFlags flags);
    void create_buffer(VkDeviceSize size,VkBufferUsageFlags usage,VkMemoryPropertyFlags flags,VkBuffer& buffer,VkDeviceMemory& memory);
    void copy_buffer(VkBuffer source,VkBuffer destination,VkDeviceSize size);
    void create_vertex_buffer(const std::vector<vertex>& vertices);
    void create_index_buffer(const std::vector<uint32_t>& indices);
protected:
    void init(VkDevice device) noexcept override;
    void upload(VkCommandBuffer command,VkPipelineLayout layout) override;
    void cleanup() override;
};
}
#endif
