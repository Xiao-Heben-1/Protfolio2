#pragma once
#include <array>
#include <filesystem>
#include <optional>
#include <string>
#include <rapidjson/document.h>
#include <tools/structures/AABB.hpp>
// 仅供配置cpp共享的JSON基础读写，不包含场景或组件业务。
namespace render::config_json {
using json=rapidjson::Value;
using allocator=rapidjson::Document::AllocatorType;
json encode(const std::string& value,allocator& alloc);
json encode(float value,allocator& alloc);
json encode(uint32_t value,allocator& alloc);
json encode(bool value,allocator& alloc);
json encode(const AABB& value,allocator& alloc);
void decode(const json& source,std::string& value);
void decode(const json& source,float& value);
void decode(const json& source,uint32_t& value);
void decode(const json& source,bool& value);
void decode(const json& source,AABB& value);
template<class T,size_t N> json encode(const std::array<T,N>& values,allocator& alloc){
    json result(rapidjson::kArrayType);
    for(const auto& value:values)result.PushBack(encode(value,alloc),alloc);
    return result;
}
template<class T,size_t N> void decode(const json& source,std::array<T,N>& values){
    for(size_t i=0;i<N;++i)decode(source[rapidjson::SizeType(i)],values[i]);
}
template<class T> void put(json& target,const char* key,const T& value,allocator& alloc){
    target.AddMember(json(key,alloc),encode(value,alloc),alloc);
}
template<class T> void put_optional(json& target,const char* key,const std::optional<T>& value,allocator& alloc){
    if(value)put(target,key,*value,alloc);
}
template<class T> void read(const json& source,const char* key,T& value){
    if(source.HasMember(key))decode(source[key],value);
}
template<class T> void read_optional(const json& source,const char* key,std::optional<T>& value){
    if(source.HasMember(key)){T result{};decode(source[key],result);value=std::move(result);}
}
void read_guid(const json& source,std::string& guid);
rapidjson::Document load(const std::filesystem::path& path,std::string& guid);
void save(const std::filesystem::path& path,const std::string& guid,const json& document);
}
