#include <core/render/data/prim_culling_data.hpp>
#include <fstream>
#include <iostream>
#include <cstring>
#include <tools/model/QEM.hpp>
#include "mesh_stream_test.hpp"

using namespace render;
static void check(VkResult r) { if(r != VK_SUCCESS) throw std::runtime_error("Vulkan call failed: " + std::to_string(r)); }
struct context {
    VkInstance instance{}; VkDevice device{}; VkPhysicalDevice physical{}; VkQueue queue{};
    VkCommandPool pool{}; VkPipeline pipeline{}; VkPipelineLayout layout{}; VkShaderModule module{};
    VkBuffer readback{}; VkDeviceMemory memory{}; void* mapped{};
    prim_culling_data culling;
    ~context() {
        if(device) {
            vkDeviceWaitIdle(device); culling.cleanup();
            vkDestroyPipeline(device,pipeline,nullptr); vkDestroyPipelineLayout(device,layout,nullptr);
            vkDestroyShaderModule(device,module,nullptr); vkDestroyCommandPool(device,pool,nullptr);
            if(mapped) vkUnmapMemory(device,memory);
            vkDestroyBuffer(device,readback,nullptr); vkFreeMemory(device,memory,nullptr);
            vkDestroyDevice(device,nullptr);
        }
        if(instance) vkDestroyInstance(instance,nullptr);
    }
};
int main(int argc,char** argv) try {
    if(argc != 2) throw std::runtime_error("Expected compiled culling shader path");
    context ctx;
    VkApplicationInfo app{VK_STRUCTURE_TYPE_APPLICATION_INFO}; app.apiVersion = VK_API_VERSION_1_3;
    VkInstanceCreateInfo instance{VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO}; instance.pApplicationInfo = &app;
    check(vkCreateInstance(&instance,nullptr,&ctx.instance));
    uint32_t count=0; check(vkEnumeratePhysicalDevices(ctx.instance,&count,nullptr));
    std::vector<VkPhysicalDevice> devices(count); check(vkEnumeratePhysicalDevices(ctx.instance,&count,devices.data()));
    uint32_t family=UINT32_MAX;
    for(auto device : devices) {
        uint32_t n=0; vkGetPhysicalDeviceQueueFamilyProperties(device,&n,nullptr);
        std::vector<VkQueueFamilyProperties> families(n); vkGetPhysicalDeviceQueueFamilyProperties(device,&n,families.data());
        for(uint32_t i=0;i<n;++i) if((families[i].queueFlags & (VK_QUEUE_GRAPHICS_BIT | VK_QUEUE_COMPUTE_BIT)) == (VK_QUEUE_GRAPHICS_BIT | VK_QUEUE_COMPUTE_BIT)) { family=i; ctx.physical=device; break; }
        if(ctx.physical) break;
    }
    if(!ctx.physical) throw std::runtime_error("No compute/graphics queue");
    const float priority=1;
    VkDeviceQueueCreateInfo queue{VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO}; queue.queueFamilyIndex=family; queue.queueCount=1; queue.pQueuePriorities=&priority;
    VkDeviceCreateInfo device{VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO}; device.queueCreateInfoCount=1; device.pQueueCreateInfos=&queue;
    check(vkCreateDevice(ctx.physical,&device,nullptr,&ctx.device)); vkGetDeviceQueue(ctx.device,family,0,&ctx.queue);
    ctx.culling.init(ctx.physical,ctx.device);
    VkPushConstantRange range{VK_SHADER_STAGE_COMPUTE_BIT,0,4};
    VkPipelineLayoutCreateInfo layout{VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO}; layout.setLayoutCount=1; layout.pSetLayouts=&ctx.culling.layout; layout.pushConstantRangeCount=1; layout.pPushConstantRanges=&range;
    check(vkCreatePipelineLayout(ctx.device,&layout,nullptr,&ctx.layout));
    std::ifstream file(argv[1],std::ios::binary|std::ios::ate);
    if(!file) throw std::runtime_error("Missing shader");
    const auto size = file.tellg(); std::vector<uint32_t> words(size_t(size)/4);
    file.seekg(0); file.read(reinterpret_cast<char*>(words.data()),size);
    VkShaderModuleCreateInfo module{VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO}; module.codeSize=words.size()*4; module.pCode=words.data();
    check(vkCreateShaderModule(ctx.device,&module,nullptr,&ctx.module));
    VkComputePipelineCreateInfo pipeline{VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO};
    pipeline.stage = {VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,nullptr,0,VK_SHADER_STAGE_COMPUTE_BIT,ctx.module,"main",nullptr}; pipeline.layout=ctx.layout;
    check(vkCreateComputePipelines(ctx.device,VK_NULL_HANDLE,1,&pipeline,nullptr,&ctx.pipeline));
    VkCommandPoolCreateInfo pool{VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO}; pool.queueFamilyIndex=family;
    check(vkCreateCommandPool(ctx.device,&pool,nullptr,&ctx.pool));
    VkBufferCreateInfo buffer{VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO}; buffer.size=512*sizeof(VkDrawIndexedIndirectCommand); buffer.usage=VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    check(vkCreateBuffer(ctx.device,&buffer,nullptr,&ctx.readback));
    VkMemoryRequirements required{}; vkGetBufferMemoryRequirements(ctx.device,ctx.readback,&required);
    VkPhysicalDeviceMemoryProperties properties{}; vkGetPhysicalDeviceMemoryProperties(ctx.physical,&properties);
    uint32_t type=UINT32_MAX;
    for(uint32_t i=0;i<properties.memoryTypeCount;++i) if((required.memoryTypeBits & (1u<<i)) &&
        (properties.memoryTypes[i].propertyFlags & (VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT|VK_MEMORY_PROPERTY_HOST_COHERENT_BIT)) == (VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT|VK_MEMORY_PROPERTY_HOST_COHERENT_BIT)) { type=i; break; }
    if(type==UINT32_MAX) throw std::runtime_error("No readback memory");
    VkMemoryAllocateInfo allocation{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO}; allocation.allocationSize=required.size; allocation.memoryTypeIndex=type;
    check(vkAllocateMemory(ctx.device,&allocation,nullptr,&ctx.memory)); check(vkBindBufferMemory(ctx.device,ctx.readback,ctx.memory,0));
    check(vkMapMemory(ctx.device,ctx.memory,0,buffer.size,0,&ctx.mapped));
    // 空帧、可见/不可见/相切、非64倍数、容量增长和复用后的写零。
    for(uint32_t n : {0u,5u,257u,1u,0u}) {
        std::vector<prim_cull_item> items(n);
        std::vector<VkDrawIndexedIndirectCommand> commands(n);
        for(uint32_t i=0;i<n;++i) {
            switch(i%5) {
                case 0: items[i].bounds_min={-.5f,-.5f,0,0}; items[i].bounds_max={.5f,.5f,.5f,0}; break;
                case 1: items[i].bounds_min={2,0,0,0}; items[i].bounds_max={3,1,1,0}; break;
                case 2: items[i].bounds_min={0,0,-1,0}; items[i].bounds_max={1,1,-.1f,0}; break;
                case 3: items[i].bounds_min={1,0,0,0}; items[i].bounds_max={2,1,1,0}; break;
                default: items[i].bounds_min={0,0,2,0}; items[i].bounds_max={1,1,3,0}; break;
            }
            commands[i]={3+i,99,7+i,-3,0}; // GPU 必须改写所有 instanceCount
        }
        ctx.culling.prepare(items,commands,glm::mat4(1));
        VkCommandBufferAllocateInfo allocate{VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO}; allocate.commandPool=ctx.pool; allocate.commandBufferCount=1; allocate.level=VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        VkCommandBuffer cmd{}; check(vkAllocateCommandBuffers(ctx.device,&allocate,&cmd));
        VkCommandBufferBeginInfo begin{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO}; check(vkBeginCommandBuffer(cmd,&begin));
        ctx.culling.dispatch(cmd,ctx.pipeline,ctx.layout);
        if(n) {
            VkMemoryBarrier barrier{VK_STRUCTURE_TYPE_MEMORY_BARRIER}; barrier.srcAccessMask=VK_ACCESS_SHADER_WRITE_BIT|VK_ACCESS_HOST_WRITE_BIT; barrier.dstAccessMask=VK_ACCESS_TRANSFER_READ_BIT;
            vkCmdPipelineBarrier(cmd,VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT|VK_PIPELINE_STAGE_HOST_BIT,VK_PIPELINE_STAGE_TRANSFER_BIT,0,1,&barrier,0,nullptr,0,nullptr);
            VkBufferCopy copy{0,0,n*sizeof(VkDrawIndexedIndirectCommand)};
            vkCmdCopyBuffer(cmd,ctx.culling.indirect_buffer(),ctx.readback,1,&copy);
            barrier.srcAccessMask=VK_ACCESS_TRANSFER_WRITE_BIT; barrier.dstAccessMask=VK_ACCESS_HOST_READ_BIT;
            vkCmdPipelineBarrier(cmd,VK_PIPELINE_STAGE_TRANSFER_BIT,VK_PIPELINE_STAGE_HOST_BIT,0,1,&barrier,0,nullptr,0,nullptr);
        }
        check(vkEndCommandBuffer(cmd));
        VkSubmitInfo submit{VK_STRUCTURE_TYPE_SUBMIT_INFO}; submit.commandBufferCount=1; submit.pCommandBuffers=&cmd;
        check(vkQueueSubmit(ctx.queue,1,&submit,VK_NULL_HANDLE)); check(vkQueueWaitIdle(ctx.queue));
        const auto* output = static_cast<const VkDrawIndexedIndirectCommand*>(ctx.mapped);
        for(uint32_t i=0;i<n;++i) {
            commands[i].instanceCount = (i%5==0 || i%5==3) ? 1 : 0;
            if(std::memcmp(&commands[i],&output[i],sizeof(commands[i]))) throw std::runtime_error("GPU culling result mismatch");
        }
        vkFreeCommandBuffers(ctx.device,ctx.pool,1,&cmd);
    }
    {
        auto cam=mesh_stream_test::make_camera();
        mesh_data original;
        for(uint32_t y=0;y<7;++y) for(uint32_t x=0;x<7;++x)
            original.vertices.emplace_back(glm::vec3(x,y,0),glm::vec2(x,y)/6.f,glm::vec3(0,0,1));
        for(uint32_t y=0;y<6;++y) for(uint32_t x=0;x<6;++x){
            const uint32_t a=y*7+x;
            original.indices.insert(original.indices.end(),{a,a+1,a+7,a+1,a+8,a+7});
        }
        original.sub_meshes={{0,uint32_t(original.indices.size()),0,5}};
        const std::array<mesh_data,3> lods{original,QEM::lod_generater(original,.5f),QEM::lod_generater(original,.35f)};
        if(lods[1].indices.size()>=lods[0].indices.size() || lods[2].indices.size()>=lods[1].indices.size())
            throw std::runtime_error("QEM LOD sizes did not decrease");

        std::array<mesh_lod_resource,3> resources;
        for(uint32_t lod=0;lod<3;++lod){
            const auto written=mesh_stream_test::write(lods[lod],lod,"lod_gpu_test_data");
            resources[lod]=mesh_lod_resource::load(written.path,64);
            std::ifstream header(written.path,std::ios::binary);std::string first;std::getline(header,first);
            if(first!=written.guid || first.size()!=32 || resources[lod].guid!=written.guid || resources[lod].lod!=lod ||
               !resources[lod].transparent || !(resources[lod].bounds==written.bounds))throw std::runtime_error("GUID/header/AABB roundtrip failed");
        }
        std::unique_ptr<mesh,void(*)(mesh*)> owner(mesh_stream_test::make_mesh(ctx.physical,ctx.device,ctx.pool,ctx.queue),mesh_stream_test::destroy);
        mesh& geometry=*owner;
        mesh_stream_test::attach(geometry,resources);
        thread_pool workers(2);
        page_manager cache({64,128,64},workers);
        for(const auto& r:resources)r.register_pages(cache);
        glm::mat4 model(1);model[0].z=-64.f;
        const auto world=geometry.get_bounds().transformed(model);
        const float half_depth=(world.get_max().z-world.get_min().z)*.5f;
        uint64_t frame=0;
        bool displayed=false;
        // 世界包围盒有384深度，最近边缘已进细档时，中心仍在粗档。
        for(float distance : {127.f,128.f,639.f,640.f,1151.f,1152.f,2175.f,639.f,127.f,-32.f}) {
            const uint32_t target=distance<128 ? 0 : distance<640 ? 1 : 2;
            mesh_stream_test::camera_position(cam,world.get_center()+glm::vec3(0,0,distance+half_depth));
            const auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(10);
            while(true){
                cache.poll(); mesh_stream_test::update(geometry,cam,world,cache,++frame);
                if(displayed && !geometry.get_vertex_buffer()) throw std::runtime_error("LOD switch lost fallback");
                displayed |= geometry.get_vertex_buffer()!=VK_NULL_HANDLE;
                if(geometry.get_vertex_buffer() && geometry.get_cur_lod()==target) break;
                if(std::chrono::steady_clock::now()>deadline) throw std::runtime_error("LOD stream did not complete");
                VkCommandBufferAllocateInfo allocate{VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
                allocate.commandPool=ctx.pool; allocate.level=VK_COMMAND_BUFFER_LEVEL_PRIMARY; allocate.commandBufferCount=1;
                VkCommandBuffer cmd{}; check(vkAllocateCommandBuffers(ctx.device,&allocate,&cmd));
                VkCommandBufferBeginInfo begin{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO}; check(vkBeginCommandBuffer(cmd,&begin));
                mesh_stream_test::upload(geometry,cmd,cache);
                check(vkEndCommandBuffer(cmd));
                VkSubmitInfo submit{VK_STRUCTURE_TYPE_SUBMIT_INFO}; submit.commandBufferCount=1; submit.pCommandBuffers=&cmd;
                check(vkQueueSubmit(ctx.queue,1,&submit,VK_NULL_HANDLE)); check(vkQueueWaitIdle(ctx.queue));
                mesh_stream_test::finish(geometry); cache.trim(frame>2 ? frame-2 : 0);
                vkFreeCommandBuffers(ctx.device,ctx.pool,1,&cmd);
                std::this_thread::yield();
            }
            if(mesh_stream_test::resident(geometry)!=1)throw std::runtime_error("Old LOD including LOD2 remained resident");
            if(mesh_stream_test::drawable(geometry)!=(distance<1152))throw std::runtime_error("Draw/preload band boundary mismatch");
            // 从真实GPU顶点/索引缓冲读回，检查页边界和顶点/索引交界的复制。
            VkCommandBufferAllocateInfo allocate{VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
            allocate.commandPool=ctx.pool; allocate.level=VK_COMMAND_BUFFER_LEVEL_PRIMARY; allocate.commandBufferCount=1;
            VkCommandBuffer cmd{}; check(vkAllocateCommandBuffers(ctx.device,&allocate,&cmd));
            VkCommandBufferBeginInfo begin{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO}; check(vkBeginCommandBuffer(cmd,&begin));
            VkMemoryBarrier barrier{VK_STRUCTURE_TYPE_MEMORY_BARRIER}; barrier.srcAccessMask=VK_ACCESS_TRANSFER_WRITE_BIT; barrier.dstAccessMask=VK_ACCESS_TRANSFER_READ_BIT;
            vkCmdPipelineBarrier(cmd,VK_PIPELINE_STAGE_TRANSFER_BIT,VK_PIPELINE_STAGE_TRANSFER_BIT,0,1,&barrier,0,nullptr,0,nullptr);
            const auto vb=resources[target].vertex_bytes,ib=resources[target].index_bytes;
            VkBufferCopy vertices{0,0,vb},indices{0,vb,ib};
            vkCmdCopyBuffer(cmd,geometry.get_vertex_buffer(),ctx.readback,1,&vertices);
            vkCmdCopyBuffer(cmd,geometry.get_index_buffer(),ctx.readback,1,&indices);
            barrier.srcAccessMask=VK_ACCESS_TRANSFER_WRITE_BIT; barrier.dstAccessMask=VK_ACCESS_HOST_READ_BIT;
            vkCmdPipelineBarrier(cmd,VK_PIPELINE_STAGE_TRANSFER_BIT,VK_PIPELINE_STAGE_HOST_BIT,0,1,&barrier,0,nullptr,0,nullptr);
            check(vkEndCommandBuffer(cmd));
            VkSubmitInfo submit{VK_STRUCTURE_TYPE_SUBMIT_INFO}; submit.commandBufferCount=1; submit.pCommandBuffers=&cmd;
            check(vkQueueSubmit(ctx.queue,1,&submit,VK_NULL_HANDLE)); check(vkQueueWaitIdle(ctx.queue));
            if(std::memcmp(ctx.mapped,lods[target].vertices.data(),vb) ||
               std::memcmp(static_cast<const std::byte*>(ctx.mapped)+vb,lods[target].indices.data(),ib))
                throw std::runtime_error("Streamed GPU geometry bytes differ from QEM output");
            if(geometry.get_data().sub_meshes.front().mat_idx!=5) throw std::runtime_error("LOD lost material slot");
            vkFreeCommandBuffers(ctx.device,ctx.pool,1,&cmd);
        }
        mesh_stream_test::transparent(geometry,false);
        if(geometry.get_lods()[0].transparent)throw std::runtime_error("Transparency metadata change failed");
        mesh_stream_test::camera_position(cam,world.get_center()+glm::vec3(0,0,2176+half_depth));
        mesh_stream_test::update(geometry,cam,world,cache,++frame);
        if(geometry.get_vertex_buffer())throw std::runtime_error("Outside buffer range was not released");
    }
    std::cout << "GPU frustum culling and three-level LOD streaming tests passed\n";
    return 0;
} catch(const std::exception& e) { std::cerr << e.what() << '\n'; return 1; }


