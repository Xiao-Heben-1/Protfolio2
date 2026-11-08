#include <file/config/resource/mesh_lod_resource.hpp>
#include <file/tools/guid.hpp>
#include <rapidjson/document.h>
#include <rapidjson/writer.h>
#include <rapidjson/stringbuffer.h>
#include <fstream>

namespace render {
namespace {
void fill_pages(mesh_lod_resource& r,uint32_t size){
    for(uint64_t pos=0;pos<r.vertex_bytes+r.index_bytes;pos+=size)
        r.pages.push_back({{r.guid,uint32_t(r.pages.size())},r.path,r.data_offset+pos,uint32_t(std::min(uint64_t(size),r.vertex_bytes+r.index_bytes-pos))});
}
}
mesh_lod_resource mesh_lod_resource::write(const mesh_data& data,const std::string& source,
    uint32_t level,bool alpha,const std::string& fingerprint,const std::filesystem::path& directory,uint32_t page_size){
    mesh_lod_resource r;r.guid=generate_guid();r.source_guid=source;r.lod=level;r.transparent=alpha;r.signature=fingerprint;
    r.type=file_type::geometry;r.name=r.guid+".prim";r.path=std::filesystem::absolute(directory/r.name);
    r.vertex_bytes=data.vertices.size()*sizeof(vertex);r.index_bytes=data.indices.size()*sizeof(uint32_t);r.sub_meshes=data.sub_meshes;
    for(const auto& v:data.vertices)r.bounds=merge(r.bounds,AABB(v.position));
    rapidjson::StringBuffer buffer;rapidjson::Writer<rapidjson::StringBuffer>w(buffer);
    w.StartObject();w.Key("source_guid");w.String(source.c_str());w.Key("signature");w.String(fingerprint.c_str());
    w.Key("lod");w.Uint(level);w.Key("transparent");w.Bool(alpha);w.Key("aabb");w.StartArray();
    for(int a=0;a<3;++a)w.Double(r.bounds.get_min()[a]);for(int a=0;a<3;++a)w.Double(r.bounds.get_max()[a]);w.EndArray();
    w.Key("vertex_bytes");w.Uint64(r.vertex_bytes);w.Key("index_bytes");w.Uint64(r.index_bytes);
    w.Key("sub_meshes");w.StartArray();for(const auto& s:data.sub_meshes){w.StartArray();w.Uint(s.front_idx);w.Uint(s.index_count);w.Int(s.vertex_off);w.Uint(s.mat_idx);w.EndArray();}w.EndArray();w.EndObject();
    std::filesystem::create_directories(directory);auto temporary=r.path;temporary+=".tmp";
    {std::ofstream out(temporary,std::ios::binary);out<<r.guid<<'\n'<<buffer.GetString()<<'\n';r.data_offset=uint64_t(out.tellp());
    out.write(reinterpret_cast<const char*>(data.vertices.data()),r.vertex_bytes);out.write(reinterpret_cast<const char*>(data.indices.data()),r.index_bytes);
    if(!out.flush())throw std::runtime_error("Cannot write geometry resource");}
    std::filesystem::rename(temporary,r.path);fill_pages(r,page_size);return r;
}
mesh_lod_resource mesh_lod_resource::load(const std::filesystem::path& file,uint32_t page_size){
    std::ifstream in(file,std::ios::binary);if(!in)throw std::runtime_error("Cannot read geometry header");
    mesh_lod_resource r;r.path=std::filesystem::absolute(file);r.name=file.filename().string();r.type=file_type::geometry;
    std::getline(in,r.guid);std::string header;std::getline(in,header);r.data_offset=uint64_t(in.tellg());
    rapidjson::Document d;d.Parse(header.c_str());r.source_guid=d["source_guid"].GetString();r.signature=d["signature"].GetString();
    r.lod=d["lod"].GetUint();r.transparent=d["transparent"].GetBool();glm::vec3 lo,hi;
    for(unsigned a=0;a<3;++a){lo[a]=d["aabb"][a].GetFloat();hi[a]=d["aabb"][a+3].GetFloat();}r.bounds=AABB(lo,hi);
    r.vertex_bytes=d["vertex_bytes"].GetUint64();r.index_bytes=d["index_bytes"].GetUint64();
    for(const auto& s:d["sub_meshes"].GetArray())r.sub_meshes.push_back({s[0].GetUint(),s[1].GetUint(),s[2].GetInt(),s[3].GetUint()});
    fill_pages(r,page_size);return r;
}
void mesh_lod_resource::register_pages(page_manager& manager)const {for(const auto& p:pages)manager.register_page(p);}
}
