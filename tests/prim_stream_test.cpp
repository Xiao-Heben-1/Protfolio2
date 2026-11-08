#include <file/config/scence_prim.hpp>
#include <file/config/resource/page_manager.hpp>
#include <file/tools/guid.hpp>
#include <tools/structures/BVH/morton.hpp>
#include <tools/structures/BVH/BVH.hpp>
#include <algorithm>
#include <map>
#include <random>
#include <cassert>
#include <fstream>
#include <iostream>
using namespace render;
static AABB box(float lo,float hi){return AABB({lo,0,0},{hi,1,1});}
static void wait_read(page_manager& pages,page_handle h){
    const auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(10);
    while(pages.query(h)->state==page_state::loading){pages.poll();assert(std::chrono::steady_clock::now()<deadline);std::this_thread::yield();}
}
static void test_upper_sah(){
    SBVH tree;
    std::map<uint32_t,AABB> reference;
    std::mt19937 random(7731);
    auto random_box=[&]{
        glm::vec3 p;for(int axis=0;axis<3;++axis)p[axis]=float(int(random()%2000)-1000);
        glm::vec3 extent;for(int axis=0;axis<3;++axis)extent[axis]=float(random()%80);
        return AABB(p,p+extent);
    };
    auto compare=[&](const AABB& bounds){
        auto actual=tree.query(bounds);std::sort(actual.begin(),actual.end());
        std::vector<uint32_t> expected;
        for(const auto& [id,box]:reference)if(box.intersects(bounds))expected.push_back(id);
        assert(actual==expected);
    };
    auto set=[&](uint32_t id,const AABB& bounds){reference.insert_or_assign(id,bounds);tree.upsert(id,bounds);};
    const AABB all({-10000,-10000,-10000},{10000,10000,10000});
    compare(all);
    // 首次16桶建树；随后增量插入、移动引发局部重建、删除及槽位复用。
    for(uint32_t id=0;id<200;++id)set(id,random_box());
    compare(all);
    for(uint32_t step=0;step<1500;++step){
        const uint32_t id=random()%400;
        if(step%4==0){tree.erase_node(id);reference.erase(id);}
        else set(id,random_box());
        compare(random_box());
        if(step%25==0)compare(all);
    }
    while(!reference.empty()){
        const auto id=reference.begin()->first;reference.erase(id);tree.erase_node(id);compare(all);
    }
    // 退化共面/重叠叶子精确过滤，以及叶子分裂后的父节点重接。
    for(uint32_t id=0;id<40;++id)set(id,AABB({0,0,0},{10,10,0}));
    compare(all);
    set(39,AABB({500,500,0},{510,510,0}));compare(AABB({499,499,-1},{511,511,1}));
    set(40,AABB({1000,1000,0},{1010,1010,0}));compare(all);
    for(uint32_t id=0;id<41;++id){tree.erase_node(id);reference.erase(id);compare(all);}
    set(999,AABB({0,0,0},{1,1,1}));compare(all);
}
int main(){
    test_upper_sah();
    assert(morton::grid_coordinate(-.1)==-1);
    scence_prim spatial;const std::array<std::string,3> guids{generate_guid(),generate_guid(),generate_guid()};
    spatial.upsert({7,box(-1,129),guids});assert(spatial.query(box(-128,255)).size()==1);
    assert(spatial.query_cell({0,0,0}).size()==1);
    spatial.upsert({7,box(512,513),guids});assert(spatial.query(box(-128,255)).empty());
    assert(spatial.query(box(512,513)).size()==1); // existing leaf refit
    thread_pool workers(4);
    parallelfor(workers,[&](size_t id){spatial.upsert({uint32_t(100+id),box(float(id)*128,float(id)*128+1),guids});},100);
    std::vector<std::future<void>> updates;
    for(uint32_t id=100;id<200;++id)updates.push_back(spatial.upsert_async({id,box(-256,-255),guids},workers));
    auto query=spatial.query_async(box(-256,12800),workers);for(auto& f:updates)f.get();query.get();
    assert(spatial.query_cell({-256,0,0}).size()==100);
    assert(spatial.query(box(-10000000,10000000)).size()==101); // BVH: no empty-cell enumeration
    assert(spatial.erase(7));assert(!spatial.erase(7));
    for(uint32_t id=100;id<150;++id)spatial.erase(id);
    assert(spatial.query(box(-256,-255)).size()==50);
    const auto root=std::filesystem::path("build/prim_stream_test_data");std::filesystem::create_directories(root);
    spatial.save_async(root/"spatial.guidscene",workers).get();
    auto restored=scence_prim::load(root/"spatial.guidscene");assert(restored.query_cell({-256,0,0}).size()==50);
    assert(restored.query(box(-256,-255))[0].lod_guids==guids);
    const auto file=root/"pages.bin";{std::ofstream out(file,std::ios::binary);out<<"abcdefghijklmnop";}
    page_manager pages({8,16,8},workers);const page_key a{guids[0],0},b{guids[0],1},bad{guids[1],0};
    pages.register_page({a,file,2,6});pages.register_page({b,file,8,8});pages.register_page({bad,file,14,8});
    const auto h=*pages.request(a,1);assert(pages.request(a,2)==h);assert(!pages.request(b));
    wait_read(pages,h);auto mapping=pages.resolve({a,1},3);
    assert(mapping && char(mapping->data()[0])=='d' && char(mapping->data()[2])=='f');
    assert(!pages.resolve({a,5},2));assert(pages.evict(h));assert(!pages.query(h));
    const auto second=*pages.request(b,4);assert(second.slot==h.slot && second.generation!=h.generation);wait_read(pages,second);
    assert(pages.used_bytes()==16 && !pages.request(a));mapping.reset();assert(pages.used_bytes()==8);
    pages.trim(5);assert(!pages.query(second));
    const auto broken=*pages.request(bad);wait_read(pages,broken);assert(pages.query(broken)->state==page_state::failed);pages.evict(broken);
    std::vector<page_handle> handles(32);parallelfor(workers,[&](size_t i){handles[i]=*pages.request(a);},handles.size());
    for(auto v:handles)assert(v==handles[0]);wait_read(pages,handles[0]);
    {page_manager temporary({8,8,8},workers);temporary.register_page({a,file,0,8});temporary.request(a);}
    std::cout<<"GUID spatial/BVH/page manager tests passed\n";
}
