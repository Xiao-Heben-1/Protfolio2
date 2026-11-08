#include <file/config/scence_prim.hpp>
#include <file/tools/guid.hpp>
#include <tools/structures/BVH/morton.hpp>
#include <tools/structures/BVH/BVH.hpp>
#include <rapidjson/document.h>
#include <rapidjson/writer.h>
#include <rapidjson/stringbuffer.h>
#include <fstream>
#include <shared_mutex>
#include <unordered_map>
#include <unordered_set>

namespace render {
namespace {
std::mutex file_mutex;
std::vector<uint64_t> cells(const AABB& box){
    const auto lo=glm::ivec3(glm::floor(box.get_min()/128.f));
    const auto hi=glm::ivec3(glm::floor(box.get_max()/128.f));
    std::vector<uint64_t> result;
    for(int z=lo.z;z<=hi.z;++z)for(int y=lo.y;y<=hi.y;++y)for(int x=lo.x;x<=hi.x;++x)
        result.push_back(morton::position_to_morton64(glm::vec3(x,y,z)*128.f));
    return result;
}
}
struct scence_prim::state {
    std::string guid=generate_guid();
    mutable std::shared_mutex mutex;
    std::unordered_map<uint32_t,prim_instance> instances;
    std::unordered_map<uint64_t,std::unordered_set<uint32_t>> cells;
    SBVH spatial;
};
scence_prim::scence_prim():state_(std::make_shared<state>()){}
void scence_prim::upsert(prim_instance instance){
    std::unique_lock lock(state_->mutex);
    auto it=state_->instances.find(instance.id);
    if(it!=state_->instances.end()){
        if(it->second.bounds==instance.bounds && it->second.lod_guids==instance.lod_guids)return;
        for(auto key:cells(it->second.bounds)){
            auto& cell=state_->cells.at(key);cell.erase(instance.id);if(cell.empty())state_->cells.erase(key);
        }
    }
    for(auto key:cells(instance.bounds))state_->cells[key].insert(instance.id);
    state_->spatial.upsert(instance.id,instance.bounds);
    state_->instances.insert_or_assign(instance.id,std::move(instance));
}
bool scence_prim::erase(uint32_t id){
    std::unique_lock lock(state_->mutex);auto it=state_->instances.find(id);if(it==state_->instances.end())return false;
    for(auto key:cells(it->second.bounds)){auto& cell=state_->cells.at(key);cell.erase(id);if(cell.empty())state_->cells.erase(key);}
    state_->spatial.erase_node(id);state_->instances.erase(it);return true;
}
std::vector<prim_instance> scence_prim::query(const AABB& box) const {
    std::unique_lock lock(state_->mutex);std::vector<prim_instance> result;
    for(auto id:state_->spatial.query(box))result.push_back(state_->instances.at(id));return result;
}
std::vector<prim_instance> scence_prim::query_cell(const glm::vec3& position) const {
    const auto key=morton::position_to_morton64(position);
    std::shared_lock lock(state_->mutex);std::vector<prim_instance> result;
    auto cell=state_->cells.find(key);if(cell!=state_->cells.end())for(auto id:cell->second)result.push_back(state_->instances.at(id));return result;
}
void scence_prim::save(const std::filesystem::path& file) const {
    rapidjson::StringBuffer buffer;rapidjson::Writer<rapidjson::StringBuffer> w(buffer);
    std::string guid;
    {std::shared_lock lock(state_->mutex);guid=state_->guid;
    w.StartObject();w.Key("instances");w.StartArray();
    for(const auto& [id,i]:state_->instances){
        w.StartObject();w.Key("id");w.Uint(id);w.Key("aabb");w.StartArray();
        for(int a=0;a<3;++a)w.Double(i.bounds.get_min()[a]);for(int a=0;a<3;++a)w.Double(i.bounds.get_max()[a]);w.EndArray();
        w.Key("lod_guids");w.StartArray();for(const auto& g:i.lod_guids)w.String(g.c_str());w.EndArray();w.EndObject();
    }w.EndArray();w.EndObject();}
    std::lock_guard lock(file_mutex);
    if(!file.parent_path().empty())std::filesystem::create_directories(file.parent_path());
    std::ofstream out(file,std::ios::binary);out<<guid<<'\n'<<buffer.GetString()<<'\n';
    if(!out.flush())throw std::runtime_error("Cannot save spatial table");
}
scence_prim scence_prim::load(const std::filesystem::path& file){
    std::lock_guard lock(file_mutex);std::ifstream in(file,std::ios::binary);
    if(!in)throw std::runtime_error("Cannot read spatial table");
    scence_prim result;std::getline(in,result.state_->guid);std::string json;std::getline(in,json);
    rapidjson::Document d;d.Parse(json.c_str());
    for(const auto& v:d["instances"].GetArray()){
        prim_instance i;i.id=v["id"].GetUint();glm::vec3 lo,hi;
        for(unsigned a=0;a<3;++a){lo[a]=v["aabb"][a].GetFloat();hi[a]=v["aabb"][a+3].GetFloat();}
        i.bounds=AABB(lo,hi);for(unsigned a=0;a<3;++a)i.lod_guids[a]=v["lod_guids"][a].GetString();result.upsert(std::move(i));
    }return result;
}
std::future<void> scence_prim::upsert_async(prim_instance i,thread_pool& p)const{return p.enqueue([self=*this,i=std::move(i)]()mutable{self.upsert(std::move(i));});}
std::future<bool> scence_prim::erase_async(uint32_t id,thread_pool& p)const{return p.enqueue([self=*this,id]()mutable{return self.erase(id);});}
std::future<std::vector<prim_instance>> scence_prim::query_async(AABB b,thread_pool& p)const{return p.enqueue([self=*this,b]{return self.query(b);});}
std::future<void> scence_prim::save_async(std::filesystem::path f,thread_pool& p)const{return p.enqueue([self=*this,f=std::move(f)]{self.save(f);});}
std::future<scence_prim> scence_prim::load_async(std::filesystem::path f,thread_pool& p){return p.enqueue([f=std::move(f)]{return load(f);});}
}
