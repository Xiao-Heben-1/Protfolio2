#include <tools/structures/BVH/BVH.hpp>
#include <algorithm>
#include <array>

namespace render {
// 移植自桌面项目SLBVH的S层：SAH/upperSAH、SAHInsert、SAHUpdate、SAHRebuild。
// 原treelet引用改为对象引用，不引入L层和OpenGL上传。
int SBVH::allocate_node(const AABB& bounds,int parent){
    int index;
    if(node_hollow.empty()){
        index=int(node_lists.size());
        node_lists.emplace_back(bounds);
    }else{
        index=node_hollow.front();node_hollow.pop();
        node_lists[index]=space_BVH_node(bounds);
    }
    node_lists[index].fa=parent;
    node_lists[index].sah=bounds.SA();
    return index;
}
void SBVH::release_node(int node){
    node_lists[node]=space_BVH_node();
    node_hollow.push(node);
}
int SBVH::SAH(std::vector<temp_node>& entries,const AABB& bounds,int start,int end,int dim){
    constexpr int bucket_count=16;
    std::array<AABB,bucket_count> buckets,prefix,suffix;
    std::array<int,bucket_count> counts{},prefix_count{},suffix_count{};
    const float minimum=bounds.get_min()[dim];
    const float inverse=1.f/(bounds.get_max()[dim]-minimum+1e-6f);
    const auto bucket_of=[&](const temp_node& entry){
        return std::min(bucket_count-1,int((entry.bounds.get_center()[dim]-minimum)*inverse*bucket_count));
    };
    for(int i=start;i<end;++i){
        const int b=bucket_of(entries[i]);
        buckets[b]=merge(buckets[b],entries[i].bounds);++counts[b];
    }
    prefix[0]=buckets[0];prefix_count[0]=counts[0];
    suffix[bucket_count-1]=buckets[bucket_count-1];suffix_count[bucket_count-1]=counts[bucket_count-1];
    for(int i=1;i<bucket_count;++i){
        prefix[i]=merge(prefix[i-1],buckets[i]);prefix_count[i]=prefix_count[i-1]+counts[i];
        const int j=bucket_count-1-i;
        suffix[j]=merge(suffix[j+1],buckets[j]);suffix_count[j]=suffix_count[j+1]+counts[j];
    }
    float best_cost=bounds.SA()*(end-start);
    int best_bucket=-1;
    for(int i=0;i<bucket_count-1;++i){
        if(!prefix_count[i] || !suffix_count[i+1])continue;
        const float cost=prefix[i].SA()*prefix_count[i]+suffix[i+1].SA()*suffix_count[i+1];
        if(cost<best_cost){best_cost=cost;best_bucket=i;}
    }
    if(best_bucket==-1)return end; // 无分裂收益时，多对象保留在同一个S层叶子。
    return int(std::partition(entries.begin()+start,entries.begin()+end,
        [&](const temp_node& entry){return bucket_of(entry)<=best_bucket;})-entries.begin());
}
int SBVH::upperSAH(std::vector<temp_node>& entries,int parent,int start,int end){
    AABB bounds;
    for(int i=start;i<end;++i)bounds=merge(bounds,entries[i].bounds);
    // 先占用父节点槽位，递归期间只保存索引，避免vector扩容使引用失效。
    const int node=allocate_node(bounds,parent);
    node_lists[node].count=uint32_t(end-start);
    const int middle=end-start==1 ? end : SAH(entries,bounds,start,end,bounds.getDim());
    if(middle==end){
        auto& ids=node_lists[node].objects;ids.reserve(end-start);
        for(int i=start;i<end;++i){
            auto& entry=objects.at(entries[i].object);
            entry.node=node;entry.offset=ids.size();ids.push_back(entries[i].object);
        }
    }else{
        const int left=upperSAH(entries,node,start,middle);
        const int right=upperSAH(entries,node,middle,end);
        node_lists[node].lc=left;node_lists[node].rc=right;
    }
    return node;
}
void SBVH::build(){
    std::vector<temp_node> entries;entries.reserve(objects.size());
    for(const auto& [id,entry]:objects)entries.push_back({entry.bounds,id});
    if(!entries.empty())root_index=upperSAH(entries,-1,0,int(entries.size()));
    rebuild_pending=false;
}
void SBVH::SAHUpdate(int node){
    for(;node!=-1;node=node_lists[node].fa){
        auto& value=node_lists[node];
        if(value.is_leaf()){
            value.bbox=AABB();
            for(auto id:value.objects)value.bbox=merge(value.bbox,objects.at(id).bounds);
            value.count=uint32_t(value.objects.size());
        }else{
            value.bbox=merge(node_lists[value.lc].bbox,node_lists[value.rc].bbox);
            value.count=node_lists[value.lc].count+node_lists[value.rc].count;
        }
        value.dirty=value.dirty || value.bbox.SA()>value.sah*1.5f;
        rebuild_pending=rebuild_pending || value.dirty;
    }
}
void SBVH::SAHInsert(uint32_t id){
    const auto bounds=objects.at(id).bounds;
    int leaf=root_index;
    while(!node_lists[leaf].is_leaf()){
        const int left=node_lists[leaf].lc,right=node_lists[leaf].rc;
        const float left_cost=merge(node_lists[left].bbox,bounds).SA()-node_lists[left].bbox.SA();
        const float right_cost=merge(node_lists[right].bbox,bounds).SA()-node_lists[right].bbox.SA();
        leaf=left_cost<right_cost ? left : right;
    }
    const auto merged=merge(node_lists[leaf].bbox,bounds);
    if(bounds.SA()+node_lists[leaf].bbox.SA()<merged.SA()){
        const int previous_parent=node_lists[leaf].fa;
        const int parent=allocate_node(merged,previous_parent);
        const int right=allocate_node(bounds,parent);
        node_lists[parent].lc=leaf;node_lists[parent].rc=right;
        node_lists[leaf].fa=parent;
        node_lists[right].objects.push_back(id);node_lists[right].count=1;
        objects.at(id).node=right;objects.at(id).offset=0;
        if(previous_parent==-1)root_index=parent;
        else if(node_lists[previous_parent].lc==leaf)node_lists[previous_parent].lc=parent;
        else node_lists[previous_parent].rc=parent;
        SAHUpdate(parent);
    }else{
        auto& entry=objects.at(id);entry.node=leaf;entry.offset=node_lists[leaf].objects.size();
        node_lists[leaf].objects.push_back(id);SAHUpdate(leaf);
    }
}
void SBVH::upsert(uint32_t id,const AABB& bounds){
    auto [it,inserted]=objects.try_emplace(id,object_record{bounds});
    if(root_index==-1){it->second.bounds=bounds;return;}
    if(inserted){SAHInsert(id);return;}
    if(it->second.bounds==bounds)return;
    it->second.bounds=bounds;SAHUpdate(it->second.node);
}
void SBVH::erase_node(uint32_t id){
    const auto it=objects.find(id);if(it==objects.end())return;
    const int leaf=it->second.node;
    if(leaf==-1){objects.erase(it);return;}
    auto& ids=node_lists[leaf].objects;
    const size_t offset=it->second.offset;
    ids[offset]=ids.back();objects.at(ids[offset]).offset=offset;ids.pop_back();objects.erase(it);
    if(!ids.empty()){SAHUpdate(leaf);return;}
    const int parent=node_lists[leaf].fa;
    if(parent==-1){root_index=-1;release_node(leaf);return;}
    const int sibling=node_lists[parent].lc==leaf ? node_lists[parent].rc : node_lists[parent].lc;
    const int grandparent=node_lists[parent].fa;
    node_lists[sibling].fa=grandparent;
    if(grandparent==-1)root_index=sibling;
    else if(node_lists[grandparent].lc==parent)node_lists[grandparent].lc=sibling;
    else node_lists[grandparent].rc=sibling;
    release_node(leaf);release_node(parent);SAHUpdate(grandparent);
}
int SBVH::SAHRebuild(int node){
    const int parent=node_lists[node].fa;
    const bool left=parent!=-1 && node_lists[parent].lc==node;
    std::vector<temp_node> entries;entries.reserve(node_lists[node].count);
    std::vector<int> stack{node};
    while(!stack.empty()){
        const int current=stack.back();stack.pop_back();
        const auto& value=node_lists[current];
        if(value.is_leaf()){
            for(auto id:value.objects)entries.push_back({objects.at(id).bounds,id});
        }else{stack.push_back(value.lc);stack.push_back(value.rc);}
        release_node(current);
    }
    const int replacement=upperSAH(entries,parent,0,int(entries.size()));
    if(parent==-1)root_index=replacement;
    else if(left)node_lists[parent].lc=replacement;
    else node_lists[parent].rc=replacement;
    return replacement;
}
void SBVH::update(int node){
    if(node_lists[node].dirty){SAHRebuild(node);return;}
    if(node_lists[node].is_leaf())return;
    update(node_lists[node].lc);update(node_lists[node].rc);
}
std::vector<uint32_t> SBVH::query(const AABB& bounds){
    if(root_index==-1)build();
    std::vector<uint32_t> result;if(root_index==-1)return result;
    if(rebuild_pending){update(root_index);rebuild_pending=false;}
    std::vector<int> stack{root_index};
    while(!stack.empty()){
        const auto& node=node_lists[stack.back()];stack.pop_back();
        if(!node.bbox.intersects(bounds))continue;
        if(node.is_leaf()){
            for(auto id:node.objects)if(objects.at(id).bounds.intersects(bounds))result.push_back(id);
        }else{stack.push_back(node.lc);stack.push_back(node.rc);}
    }
    return result;
}
}
