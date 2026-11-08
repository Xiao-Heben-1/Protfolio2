#include <file/file_system.hpp>
#include <file/tools/guid.hpp>
#include <cassert>
#include <fstream>
#include <iostream>
namespace render {
class geometry_cache_test {
public:
    static auto load(file_system& fs,const std::filesystem::path& source,const std::function<mesh_data()>& build,page_manager& pages){
        return fs.get_scan().load_mesh_resources(source,false,build,pages);
    }
    static void rescan(file_system& fs){fs.get_scan().scan();}
};
}
using namespace render;
void test_config_resources();
void test_render_pass_configs();
int main(){
    test_render_pass_configs();
    test_config_resources();
    const auto root=std::filesystem::absolute(std::filesystem::path("geometry_cache_test_data")/generate_guid());
    workspace_config config;config.workspace_root=root;config.library_root="../"+root.filename().string()+"_library";
    std::filesystem::create_directories(root);
    const auto source=root/"grid.obj";
    {std::ofstream out(source);out<<"# source fingerprint fixture\n";}
    mesh_data original;
    for(uint32_t y=0;y<7;++y)for(uint32_t x=0;x<7;++x)
        original.vertices.emplace_back(glm::vec3(x,y,0),glm::vec2(x,y)/6.f,glm::vec3(0,0,1));
    for(uint32_t y=0;y<6;++y)for(uint32_t x=0;x<6;++x){const auto a=y*7+x;original.indices.insert(original.indices.end(),{a,a+1,a+7,a+1,a+8,a+7});}
    original.sub_meshes={{0,uint32_t(original.indices.size()),0,5}};
    std::array<std::string,3> guids;
    std::string source_guid;
    uint32_t builds=0;
    auto builder=[&]{++builds;return original;};
    thread_pool workers(2);page_manager pages({},workers);
    {
        file_system fs(config);
        const auto first=geometry_cache_test::load(fs,"grid.obj",builder,pages);
        source_guid=first[0].source_guid;
        assert(builds==1 && fs.get_geometry_files().size()==3);
        for(size_t i=0;i<3;++i){guids[i]=first[i].guid;assert(guids[i].size()==32);
            assert(fs.get_scan().get_library_path(guids[i])==first[i].path);
            assert(fs.get_geometry_files().at(guids[i]).path.begin()->string()=="geometry");
        }
        auto again=geometry_cache_test::load(fs,"grid.obj",builder,pages);
        assert(builds==1);
        for(size_t i=0;i<3;++i)assert(again[i].guid==guids[i]);
        geometry_cache_test::rescan(fs);
        const auto& info=fs.get_files().at(source_guid);
        assert(info.lod_guids==guids && info.bounds==AABB({0,0,0},{6,6,0}));
    }
    {
        file_system fs(config);
        assert(fs.get_files().at(source_guid).lod_guids==guids);
        const auto again=geometry_cache_test::load(fs,"grid.obj",builder,pages);
        assert(builds==1 && again[0].guid==guids[0]);
        // 修改源内容后重新构建，旧GUID不再错误地挂到源file_info上。
        {std::ofstream out(source,std::ios::app);out<<"# changed\n";}
        geometry_cache_test::rescan(fs);
        assert(fs.get_files().at(source_guid).lod_guids[0].empty());
        const auto changed=geometry_cache_test::load(fs,"grid.obj",builder,pages);
        assert(builds==2 && changed[0].guid!=guids[0]);
        assert(fs.get_files().at(source_guid).lod_guids[0]==changed[0].guid);
        fs.get_scan().delete_file(source_guid);
        assert(fs.get_geometry_files().empty());
        assert(fs.get_files().empty());
    }
    std::cout<<"Geometry cache GUID, reuse, restart, rescan and invalidation tests passed\n";
}
