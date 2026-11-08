#include <file/config/config_json.hpp>
#include <rapidjson/istreamwrapper.h>
#include <rapidjson/prettywriter.h>
#include <rapidjson/stringbuffer.h>
#include <fstream>
namespace render::config_json {
json encode(const std::string& value,allocator& alloc){return json(value.data(),rapidjson::SizeType(value.size()),alloc);}
json encode(float value,allocator&){return json(double(value));}
json encode(uint32_t value,allocator&){return json(value);}
json encode(bool value,allocator&){return json(value);}
json encode(const AABB& value,allocator& alloc){
    const auto& lo=value.get_min();const auto& hi=value.get_max();
    return encode(std::array<float,6>{lo.x,lo.y,lo.z,hi.x,hi.y,hi.z},alloc);
}
void decode(const json& source,std::string& value){if(source.IsNull()){value.clear();return;}value.assign(source.GetString(),source.GetStringLength());}
void decode(const json& source,float& value){value=source.GetFloat();}
void decode(const json& source,uint32_t& value){value=source.GetUint();}
void decode(const json& source,bool& value){value=source.GetBool();}
void decode(const json& source,AABB& value){
    std::array<float,6> values;decode(source,values);
    value=AABB({values[0],values[1],values[2]},{values[3],values[4],values[5]});
}
void read_guid(const json& source,std::string& guid){
    read(source,source.HasMember("guid") ? "guid" : "uid",guid);
}
rapidjson::Document load(const std::filesystem::path& path,std::string& guid){
    std::ifstream input(path,std::ios::binary);
    input>>std::ws;
    // 兼容旧版纯JSON；新文件第一行为GUID。
    if(input.peek()!='{'){std::getline(input,guid);if(!guid.empty() && guid.back()=='\r')guid.pop_back();}
    rapidjson::IStreamWrapper stream(input);
    rapidjson::Document document;document.ParseStream(stream);return document;
}
void save(const std::filesystem::path& path,const std::string& guid,const json& document){
    rapidjson::StringBuffer buffer;rapidjson::PrettyWriter<rapidjson::StringBuffer> writer(buffer);
    document.Accept(writer);
    std::ofstream output;
    output.exceptions(std::ios::failbit|std::ios::badbit);
    output.open(path,std::ios::binary|std::ios::trunc);
    output<<guid<<'\n';output.write(buffer.GetString(),buffer.GetSize());output.put('\n');output.close();
}
}
