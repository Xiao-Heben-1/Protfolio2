#include <core/component/mesh/mesh.hpp>
#include <core/component/component.hpp>
#include <core/object/object.hpp>
#include <cstring>
#include <core/component/camera/camera.hpp>
namespace render{
mesh::mesh(object* owner,VkPhysicalDevice _physical_device,VkDevice _device,VkCommandPool _command_pool,VkQueue _graphics_queue):
    component(owner,2),
    physical_device(_physical_device),command_pool(_command_pool),graphics_queue(_graphics_queue){
    init(_device);
    if(owner != nullptr) owner->mesh = this;
}

void mesh::init(VkDevice _device) noexcept{
    component::init(_device);
}

void mesh::cleanup(){
    if(device == VK_NULL_HANDLE) return;
    for(uint32_t lod=0;lod<3;++lod) release_lod(lod);
    for(auto page : staging) { vkDestroyBuffer(device,page.buffer,nullptr); vkFreeMemory(device,page.memory,nullptr); }
    staging.clear();
    vkDestroyBuffer(device,index_buffer,nullptr);
    vkFreeMemory(device,index_memory,nullptr);

    vkDestroyBuffer(device,vertex_buffer,nullptr);
    vkFreeMemory(device,vertex_memory,nullptr);

    index_buffer = VK_NULL_HANDLE;
    index_memory = VK_NULL_HANDLE;

    vertex_buffer = VK_NULL_HANDLE;
    vertex_memory = VK_NULL_HANDLE;
}

mesh::~mesh(){
    cleanup();
}

void mesh::create(mesh_data&& _data){
    data = std::move(_data);//保存 mesh 数据
    cur_lod=0;
    bounds=AABB();
    for(const auto& v:data.vertices) bounds=merge(bounds,AABB(v.position));
    create_vertex_buffer(data.vertices);
    create_index_buffer(data.indices);
}

void mesh::upload(VkCommandBuffer command_buffer,VkPipelineLayout pipeline_layout){
    VkDeviceSize offset = 0;
    const auto vertices = get_vertex_buffer();
    vkCmdBindVertexBuffers(command_buffer,0,1,&vertices,&offset);
    vkCmdBindIndexBuffer(command_buffer,get_index_buffer(),0,VK_INDEX_TYPE_UINT32);
}
uint32_t mesh::find_memory_type(uint32_t type_filter,VkMemoryPropertyFlags properties){
    VkPhysicalDeviceMemoryProperties memory_properties;
    vkGetPhysicalDeviceMemoryProperties(physical_device,&memory_properties);

    for(uint32_t i = 0;i < memory_properties.memoryTypeCount;i++){
        if((type_filter & (1 << i)) && (memory_properties.memoryTypes[i].propertyFlags & properties) == properties){
            return i;
        }
    }
    return -1;
}


void mesh::create_buffer(VkDeviceSize size,VkBufferUsageFlags usage,VkMemoryPropertyFlags properties,VkBuffer& buffer,VkDeviceMemory& memory){
    VkBufferCreateInfo buffer_info{};
    buffer_info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    buffer_info.size = size;
    buffer_info.usage = usage;
    buffer_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    vkCreateBuffer(device,&buffer_info,nullptr,&buffer);

    VkMemoryRequirements memory_requirements;
    vkGetBufferMemoryRequirements(device,buffer,&memory_requirements);

    VkMemoryAllocateInfo allocate_info{};
    allocate_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocate_info.allocationSize = memory_requirements.size;
    allocate_info.memoryTypeIndex = find_memory_type(memory_requirements.memoryTypeBits,properties);

    vkAllocateMemory(device,&allocate_info,nullptr,&memory);
    vkBindBufferMemory(device,buffer,memory,0);
}


void mesh::copy_buffer(VkBuffer src_buffer,VkBuffer dst_buffer,VkDeviceSize size){
    VkCommandBufferAllocateInfo allocate_info{};
    allocate_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocate_info.commandPool = command_pool;
    allocate_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocate_info.commandBufferCount = 1;

    VkCommandBuffer command_buffer;
    vkAllocateCommandBuffers(device,&allocate_info,&command_buffer);

    VkCommandBufferBeginInfo begin_info{};
    begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    begin_info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

    vkBeginCommandBuffer(command_buffer,&begin_info);

    VkBufferCopy copy_region{};
    copy_region.srcOffset = 0;
    copy_region.dstOffset = 0;
    copy_region.size = size;

    vkCmdCopyBuffer(command_buffer,src_buffer,dst_buffer,1,&copy_region);

    vkEndCommandBuffer(command_buffer);

    VkSubmitInfo submit_info{};
    submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submit_info.commandBufferCount = 1;
    submit_info.pCommandBuffers = &command_buffer;

    vkQueueSubmit(graphics_queue,1,&submit_info,VK_NULL_HANDLE);
    vkQueueWaitIdle(graphics_queue);

    vkFreeCommandBuffers(device,command_pool,1,&command_buffer);
}


void mesh::create_vertex_buffer(const std::vector<vertex>& vertices){
    VkDeviceSize buffer_size = sizeof(vertex) * vertices.size();

    VkBuffer staging_buffer;
    VkDeviceMemory staging_memory;

    create_buffer(buffer_size,VK_BUFFER_USAGE_TRANSFER_SRC_BIT,VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,staging_buffer,staging_memory);

    void* data;
    vkMapMemory(device,staging_memory,0,buffer_size,0,&data);
    memcpy(data,vertices.data(),buffer_size);
    vkUnmapMemory(device,staging_memory);

    create_buffer(buffer_size,VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,vertex_buffer,vertex_memory);

    copy_buffer(staging_buffer,vertex_buffer,buffer_size);

    vkDestroyBuffer(device,staging_buffer,nullptr);
    vkFreeMemory(device,staging_memory,nullptr);
}


void mesh::create_index_buffer(const std::vector<uint32_t>& indices){
    VkDeviceSize buffer_size = sizeof(uint32_t) * indices.size();

    VkBuffer staging_buffer;
    VkDeviceMemory staging_memory;

    create_buffer(buffer_size,VK_BUFFER_USAGE_TRANSFER_SRC_BIT,VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,staging_buffer,staging_memory);

    void* data;
    vkMapMemory(device,staging_memory,0,buffer_size,0,&data);
    memcpy(data,indices.data(),buffer_size);
    vkUnmapMemory(device,staging_memory);

    create_buffer(buffer_size,VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT,VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,index_buffer,index_memory);

    copy_buffer(staging_buffer,index_buffer,buffer_size);
    vkDestroyBuffer(device,staging_buffer,nullptr);
    vkFreeMemory(device,staging_memory,nullptr);
}


const mesh_data& mesh::get_data()const noexcept{return data;}
const AABB& mesh::get_bounds()const noexcept{return bounds;}
const std::array<mesh_lod_resource,3>& mesh::get_lods()const noexcept{return lods;}
int mesh::get_cur_lod()const noexcept{return cur_lod;}
const std::string& mesh::get_guid()const noexcept{return lods[cur_lod<0 ? 0 : cur_lod].guid;}
VkBuffer mesh::get_vertex_buffer()const noexcept{return streamed ? (cur_lod<0 ? VK_NULL_HANDLE : gpu_lods[cur_lod].vertices) : vertex_buffer;}
VkBuffer mesh::get_index_buffer()const noexcept{return streamed ? (cur_lod<0 ? VK_NULL_HANDLE : gpu_lods[cur_lod].indices) : index_buffer;}
void mesh::create_streamed(std::array<mesh_lod_resource,3> resources){
    lods=std::move(resources);streamed=true;cur_lod=-1;bounds=AABB();
    for(const auto& r:lods)bounds=merge(bounds,r.bounds);
}
void mesh::release_lod(uint32_t level){
    auto& gpu=gpu_lods[level];
    vkDestroyBuffer(device,gpu.vertices,nullptr);vkFreeMemory(device,gpu.vertex_memory,nullptr);
    vkDestroyBuffer(device,gpu.indices,nullptr);vkFreeMemory(device,gpu.index_memory,nullptr);gpu={};
}
void mesh::release_stream(){
    if(!streamed)return;
    for(uint32_t i=0;i<3;++i)release_lod(i);
    cur_lod=-1;drawable=false;data.sub_meshes.clear();
}
void mesh::update_stream(const camera& cam,const AABB& world,page_manager& pages,uint64_t frame){
    const auto& position=cam.get_position();
    const auto nearest=glm::clamp(position,world.get_min(),world.get_max());
    const float distance=glm::length(nearest-position);
    const auto& dis=camera::dis;
    const float draw_end=dis[0]+dis[1]+dis[2];
    if(distance>=draw_end+dis[3]){release_stream();drawable=false;return;}
    drawable=distance<draw_end && cam.get_frustum().intersects(world);
    if(!streamed)return;
    const uint32_t target=distance<dis[0] ? 0 : distance<dis[0]+dis[1] ? 1 : 2;
    request_lod(target,pages,frame);
}
void mesh::request_lod(uint32_t target,page_manager& pages,uint64_t frame){
    if(gpu_lods[target].ready)cur_lod=int(target);
    else if(cur_lod<0 && gpu_lods[2].ready)cur_lod=2;
    const uint32_t loading=cur_lod<0 ? 2 : target;
    // 只保留当前绘制档和正在加载的目标档，LOD2也在切换后释放。
    for(uint32_t other=0;other<3;++other)if(int(other)!=cur_lod && other!=loading)release_lod(other);
    if(cur_lod>=0)data.sub_meshes=lods[cur_lod].sub_meshes;
    auto& gpu=gpu_lods[loading];const auto& resource=lods[loading];if(gpu.ready)return;
    if(gpu.pending.empty()){gpu.pending.resize(resource.pages.size());gpu.uploaded.resize(resource.pages.size());}
    for(size_t i=0;i<resource.pages.size();++i){
        if(gpu.uploaded[i])continue;
        gpu.pending[i]=pages.request(resource.pages[i].key,frame);
        if(!gpu.pending[i])break;
    }
}
void mesh::stream_upload(VkCommandBuffer command,page_manager& pages){
    if(!streamed)return;
    for(uint32_t level=0;level<3;++level){
        auto& gpu=gpu_lods[level];const auto& resource=lods[level];
        for(size_t i=0;i<gpu.pending.size();++i){
            if(!gpu.pending[i] || gpu.uploaded[i])continue;
            auto page=pages.query(*gpu.pending[i]);
            if(!page){gpu.pending[i].reset();continue;}
            if(page->state==page_state::failed)throw std::runtime_error(page->error);
            if(page->state!=page_state::ready)continue;
            if(!gpu.vertices){
                create_buffer(resource.vertex_bytes,VK_BUFFER_USAGE_TRANSFER_DST_BIT|VK_BUFFER_USAGE_TRANSFER_SRC_BIT|VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,gpu.vertices,gpu.vertex_memory);
                create_buffer(resource.index_bytes,VK_BUFFER_USAGE_TRANSFER_DST_BIT|VK_BUFFER_USAGE_TRANSFER_SRC_BIT|VK_BUFFER_USAGE_INDEX_BUFFER_BIT,VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,gpu.indices,gpu.index_memory);
            }
            staging_page upload{};
            create_buffer(page->bytes,VK_BUFFER_USAGE_TRANSFER_SRC_BIT,VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT|VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,upload.buffer,upload.memory);
            void* mapped=nullptr;vkMapMemory(device,upload.memory,0,page->bytes,0,&mapped);
            std::memcpy(mapped,page->data.get(),page->bytes);vkUnmapMemory(device,upload.memory);staging.push_back(upload);
            const uint64_t offset=resource.pages[i].offset-resource.data_offset;
            const uint64_t vertex_count=offset<resource.vertex_bytes ? std::min(uint64_t(page->bytes),resource.vertex_bytes-offset) : 0;
            if(vertex_count){VkBufferCopy copy{0,offset,vertex_count};vkCmdCopyBuffer(command,upload.buffer,gpu.vertices,1,&copy);}
            if(vertex_count<page->bytes){VkBufferCopy copy{vertex_count,offset+vertex_count-resource.vertex_bytes,page->bytes-vertex_count};vkCmdCopyBuffer(command,upload.buffer,gpu.indices,1,&copy);}
            gpu.uploaded[i]=1;pages.evict(*gpu.pending[i]);gpu.pending[i].reset();
        }
    }
}
void mesh::finish_stream(){
    if(!streamed)return;
    for(auto& gpu:gpu_lods){
        for(auto& state:gpu.uploaded)if(state==1)state=2;
        if(!gpu.uploaded.empty())gpu.ready=std::all_of(gpu.uploaded.begin(),gpu.uploaded.end(),[](auto state){return state==2;});
    }
    for(auto page:staging){vkDestroyBuffer(device,page.buffer,nullptr);vkFreeMemory(device,page.memory,nullptr);}staging.clear();
}
void mesh::set_transparent(bool value){for(auto& r:lods)r.transparent=value;}
}
