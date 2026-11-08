#include <file/config/workspace_config.hpp>
#include <file/config/config_json.hpp>
#include <rapidjson/istreamwrapper.h>
#include <rapidjson/prettywriter.h>
#include <rapidjson/ostreamwrapper.h>
#include <fstream>
#include <file/tools/file_path.hpp>
#include <windows.h>
namespace render {
namespace {
void read_path(const config_json::json& json,const char* key,std::filesystem::path& path){
    if(json.HasMember(key))path=path_from_utf8(json[key].GetString());
}
}
void workspace_config::resolve(const std::filesystem::path& base){
    const auto rooted=[](const auto& base,const auto& path){
        auto value=(base/path).lexically_normal();
        if(value.has_relative_path() && value.filename().empty())value=value.parent_path();
        return value;
    };
    workspace_root=rooted(base,workspace_root);
    library_root=rooted(workspace_root,library_root);
    source_root=rooted(workspace_root,source_root);
    if(!engine_root.empty())engine_root=rooted(workspace_root,engine_root);
    builtin_shader_root=rooted(library_root,builtin_shader_root);
    for(auto& path:shader_include_roots)path=(workspace_root/path).lexically_normal();
    if(shader_compiler.empty()){
        const auto size=GetEnvironmentVariableW(L"VULKAN_SDK",nullptr,0);
        if(size){std::wstring sdk(size,L'\0');GetEnvironmentVariableW(L"VULKAN_SDK",sdk.data(),size);sdk.resize(size-1);
            shader_compiler=std::filesystem::path(sdk)/"Bin"/"glslc.exe";
        }else shader_compiler="glslc.exe";
    }else if(shader_compiler.has_parent_path())shader_compiler=(workspace_root/shader_compiler).lexically_normal();
}
workspace_config workspace_config::load(const std::filesystem::path& path){
    workspace_config result;
    std::ifstream input(path);rapidjson::IStreamWrapper stream(input);rapidjson::Document json;json.ParseStream(stream);
    read_path(json,"workspace_root",result.workspace_root);read_path(json,"library_root",result.library_root);
    read_path(json,"source_root",result.source_root);
    read_path(json,"engine_root",result.engine_root);
    read_path(json,"startup_scene",result.startup_scene);
    read_path(json,"builtin_shader_root",result.builtin_shader_root);read_path(json,"shader_compiler",result.shader_compiler);
    if(json.HasMember("shader_include_roots")){
        result.shader_include_roots.clear();
        for(const auto& item:json["shader_include_roots"].GetArray())result.shader_include_roots.push_back(path_from_utf8(item.GetString()));
    }
    result.resolve(std::filesystem::absolute(path).parent_path());
    return result;
}
void workspace_config::save(const std::filesystem::path& path) const{
    rapidjson::Document json(rapidjson::kObjectType);auto& alloc=json.GetAllocator();
    config_json::put(json,"workspace_root",path_to_utf8(workspace_root),alloc);
    config_json::put(json,"source_root",path_to_utf8(source_root),alloc);
    config_json::put(json,"engine_root",path_to_utf8(engine_root),alloc);
    config_json::put(json,"startup_scene",path_to_utf8(startup_scene),alloc);
    config_json::put(json,"library_root",path_to_utf8(library_root),alloc);
    config_json::put(json,"builtin_shader_root",path_to_utf8(builtin_shader_root),alloc);
    config_json::put(json,"shader_compiler",path_to_utf8(shader_compiler),alloc);
    config_json::json includes(rapidjson::kArrayType);
    for(const auto& include:shader_include_roots)includes.PushBack(config_json::encode(path_to_utf8(include),alloc),alloc);
    json.AddMember("shader_include_roots",includes,alloc);
    std::ofstream output(path);output.exceptions(std::ios::failbit|std::ios::badbit);
    rapidjson::OStreamWrapper stream(output);rapidjson::PrettyWriter<rapidjson::OStreamWrapper> writer(stream);json.Accept(writer);
}
}
