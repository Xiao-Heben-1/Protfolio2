#pragma once
#include <core/component/mesh/mesh.hpp>
#include <core/component/camera/camera.hpp>
namespace render {
// 测试友元只存在于tests，不把组件内部操作开放到产品接口。
class mesh_stream_test {
public:
    static camera make_camera(){return camera();}
    static void camera_position(camera& c,glm::vec3 p){c.set_position(p);c.set_direction({0,0,-1});c.update();}
    static mesh* make_mesh(VkPhysicalDevice p,VkDevice d,VkCommandPool cp,VkQueue q){return new mesh(nullptr,p,d,cp,q);}
    static void destroy(mesh* m){delete m;}
    static void attach(mesh& m,std::array<mesh_lod_resource,3> r){m.create_streamed(std::move(r));}
    static void update(mesh& m,const camera& c,page_manager& p,uint64_t frame){m.update_stream(c,m.bounds,p,frame);}
    static void update(mesh& m,const camera& c,const AABB& world,page_manager& p,uint64_t frame){m.update_stream(c,world,p,frame);}
    static void upload(mesh& m,VkCommandBuffer c,page_manager& p){m.stream_upload(c,p);}
    static void finish(mesh& m){m.finish_stream();}
    static void transparent(mesh& m,bool v){m.set_transparent(v);}
    static void release(mesh& m){m.release_stream();}
    static bool drawable(const mesh& m){return m.drawable;}
    static size_t resident(const mesh& m){size_t n=0;for(const auto& g:m.gpu_lods)n+=g.vertices!=VK_NULL_HANDLE;return n;}
    static mesh_lod_resource write(const mesh_data& d,uint32_t lod,const std::filesystem::path& dir){
        return mesh_lod_resource::write(d,"source-guid",lod,true,"test-source-fingerprint",dir,64);
    }
};
}
