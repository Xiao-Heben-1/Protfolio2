#include <file/config/texture_config.hpp>
#include <file/tools/library_cache.hpp>
#include <rapidjson/document.h>
#include <rapidjson/prettywriter.h>
#include <rapidjson/stringbuffer.h>
#include <fstream>
#include <stdexcept>

namespace render{
    texture_config texture_config::load(const std::filesystem::path& path){
        const auto bytes = library_cache::read(path);
        rapidjson::Document json;
        json.Parse(reinterpret_cast<const char*>(bytes.data()),bytes.size());
        if(json.HasParseError() || !json.IsObject()) throw std::runtime_error("Invalid texture_config: " + path.string());
        texture_config config;
        auto string = [&](const char* name,std::string& out){if(json.HasMember(name) && json[name].IsString()) out = json[name].GetString();};
        auto boolean = [&](const char* name,bool& out){if(json.HasMember(name) && json[name].IsBool()) out = json[name].GetBool();};
        auto number = [&](const char* name,uint32_t& out){if(json.HasMember(name) && json[name].IsUint()) out = json[name].GetUint();};
        string("uid",config.guid); string("name",config.name);
        string("format",config.format); string("resolved_format",config.resolved_format); string("signature",config.signature);
        boolean("srgb",config.srgb); boolean("generate_mips",config.generate_mips); boolean("flip_y",config.flip_y);
        boolean("has_transparency",config.has_transparency);
        boolean("alpha_weighted_mips",config.alpha_weighted_mips);
        number("version",config.version); number("width",config.width); number("height",config.height); number("mip_levels",config.mip_levels);
        return config;
    }
    void texture_config::save(const std::filesystem::path& path,const std::string& source_path,const std::string& library_path) const{
        rapidjson::StringBuffer buffer;
        rapidjson::PrettyWriter<rapidjson::StringBuffer> json(buffer);
        json.StartObject();
        auto string = [&](const char* key,const std::string& value){json.Key(key); json.String(value.data(),static_cast<rapidjson::SizeType>(value.size()));};
        auto boolean = [&](const char* key,bool value){json.Key(key); json.Bool(value);};
        auto number = [&](const char* key,uint32_t value){json.Key(key); json.Uint(value);};
        number("version",version); string("uid",guid); string("name",name);
        string("source_path",source_path); string("library_path",library_path);
        string("format",format); boolean("srgb",srgb); boolean("generate_mips",generate_mips); boolean("flip_y",flip_y);
        number("width",width); number("height",height); number("mip_levels",mip_levels);
        boolean("has_transparency",has_transparency); string("resolved_format",resolved_format); string("signature",signature);
        boolean("alpha_weighted_mips",alpha_weighted_mips);
        json.EndObject();
        std::filesystem::create_directories(path.parent_path());
        auto temporary = path; temporary += ".tmp";
        {std::ofstream output(temporary,std::ios::binary); if(!output.write(buffer.GetString(),buffer.GetSize())) throw std::runtime_error("Cannot save texture_config");}
        library_cache::commit(temporary,path);
    }
}

