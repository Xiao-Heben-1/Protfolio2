#include <file/config/resource/page_manager.hpp>
#include <tools/pools/page_pool.hpp>
#include <unordered_map>
#include <fstream>
#include <chrono>

namespace render {
struct page_manager::implementation {
    struct record { page_view view; std::future<page_pool::lease> read; };
    struct slot { uint64_t generation=1; std::unique_ptr<record> value; };
    page_budget budget;
    thread_pool& workers;
    page_pool pool;
    mutable std::mutex mutex;
    std::unordered_map<std::string,std::unordered_map<uint32_t,page_descriptor>> directory;
    std::unordered_map<std::string,std::unordered_map<uint32_t,page_handle>> lookup;
    std::vector<slot> records;
    std::vector<uint32_t> free;
    size_t inflight=0;
    implementation(page_budget b,thread_pool& w):budget(b),workers(w),pool(b.page_bytes,b.cpu_bytes){}
    record* find(page_handle h) const {
        return h.slot<records.size() && records[h.slot].generation==h.generation ? records[h.slot].value.get() : nullptr;
    }
    bool evict(page_handle h){
        auto* r=find(h); if(!r || r->view.state==page_state::loading) return false;
        auto& group=lookup.at(r->view.key.guid); group.erase(r->view.key.page);
        if(group.empty()) lookup.erase(r->view.key.guid);
        records[h.slot].value.reset(); ++records[h.slot].generation; free.push_back(h.slot); return true;
    }
};
page_manager::page_manager(page_budget b,thread_pool& w):impl(std::make_unique<implementation>(b,w)){}
page_manager::~page_manager()=default;
std::span<const std::byte> page_mapping::data() const { return owner ? std::span<const std::byte>(owner.get()+offset,bytes) : std::span<const std::byte>{}; }
void page_manager::register_page(page_descriptor p){
    p.file=std::filesystem::absolute(p.file);
    std::lock_guard lock(impl->mutex); impl->directory[p.key.guid].insert_or_assign(p.key.page,std::move(p));
}
std::optional<page_handle> page_manager::request(const page_key& key,uint64_t frame){
    auto& s=*impl; std::lock_guard lock(s.mutex);
    auto group=s.lookup.find(key.guid);
    if(group!=s.lookup.end()) if(auto it=group->second.find(key.page);it!=group->second.end()){
        s.find(it->second)->view.last_frame=frame; return it->second;
    }
    if(s.inflight+s.budget.page_bytes>s.budget.inflight_bytes) return std::nullopt;
    auto memory=s.pool.acquire(); if(!memory) return std::nullopt;
    const auto desc=s.directory.at(key.guid).at(key.page);
    uint32_t index;
    if(s.free.empty()){index=uint32_t(s.records.size());s.records.emplace_back();}
    else {index=s.free.back();s.free.pop_back();}
    const page_handle handle{index,s.records[index].generation};
    auto r=std::make_unique<implementation::record>();
    r->view={handle,key,page_state::loading,{},desc.bytes,frame,{}};
    try {
        s.lookup[key.guid].emplace(key.page,handle);
        r->read=s.workers.enqueue([desc,memory=std::move(memory)]() mutable {
            std::ifstream input(desc.file,std::ios::binary);
            if(!input.seekg(desc.offset) || !input.read(reinterpret_cast<char*>(memory.get()),desc.bytes))
                throw std::runtime_error("Cannot read geometry page: "+desc.file.string());
            return std::move(memory);
        });
    } catch(...) {s.lookup[key.guid].erase(key.page);s.free.push_back(index);throw;}
    s.records[index].value=std::move(r);s.inflight+=s.budget.page_bytes;return handle;
}
void page_manager::poll(){
    auto& s=*impl;std::lock_guard lock(s.mutex);
    for(auto& slot:s.records) if(slot.value){
        auto& r=*slot.value;
        if(r.view.state!=page_state::loading || r.read.wait_for(std::chrono::seconds(0))!=std::future_status::ready) continue;
        s.inflight-=s.budget.page_bytes;
        try {r.view.data=r.read.get();r.view.state=page_state::ready;}
        catch(const std::exception& e){r.view.state=page_state::failed;r.view.error=e.what();}
    }
}
std::optional<page_view> page_manager::query(page_handle h) const {
    std::lock_guard lock(impl->mutex);auto* r=impl->find(h);return r ? std::optional(r->view) : std::nullopt;
}
std::optional<page_mapping> page_manager::resolve(const page_address& address,uint32_t bytes) const {
    std::lock_guard lock(impl->mutex);
    auto g=impl->lookup.find(address.key.guid);if(g==impl->lookup.end())return std::nullopt;
    auto p=g->second.find(address.key.page);if(p==g->second.end())return std::nullopt;
    const auto& v=impl->find(p->second)->view;
    if(!v.data || address.offset>v.bytes || bytes>v.bytes-address.offset)return std::nullopt;
    return page_mapping{v.data,address.offset,bytes};
}
bool page_manager::evict(page_handle h){std::lock_guard lock(impl->mutex);return impl->evict(h);}
void page_manager::trim(uint64_t frame){
    std::lock_guard lock(impl->mutex);
    for(auto& s:impl->records)if(s.value && s.value->view.last_frame<frame)impl->evict(s.value->view.handle);
}
size_t page_manager::used_bytes() const{return impl->pool.used_bytes();}
}
