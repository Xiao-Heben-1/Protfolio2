#include <file/file_data/render_pass_data.hpp>
#include <file/file_scan.hpp>
#include <algorithm>
#include <cctype>
namespace render {
void render_pass_data::register_data(const std::string& guid){
    const auto path=scan->get_source_path(guid);
    auto extension=path.extension().string();
    std::transform(extension.begin(),extension.end(),extension.begin(),[](unsigned char c){return char(std::tolower(c));});
    if(extension==".pass"){auto value=pass_config::load(path);value.guid=guid;passes.insert_or_assign(guid,std::move(value));}
    else if(extension==".shader"){auto value=shader_config::load(path);value.guid=guid;shaders.insert_or_assign(guid,std::move(value));}
    else{auto value=sub_shader_config::load(path);value.guid=guid;sub_shaders.insert_or_assign(guid,std::move(value));}
}
void render_pass_data::remove_data(const std::string& guid){passes.erase(guid);shaders.erase(guid);sub_shaders.erase(guid);}
void render_pass_data::cleanup() noexcept{passes.clear();shaders.clear();sub_shaders.clear();}
pass_config* render_pass_data::get_pass(const std::string& guid){auto it=passes.find(guid);return it==passes.end() ? nullptr : &it->second;}
const pass_config* render_pass_data::get_pass(const std::string& guid) const{auto it=passes.find(guid);return it==passes.end() ? nullptr : &it->second;}
shader_config* render_pass_data::get_shader(const std::string& guid){auto it=shaders.find(guid);return it==shaders.end() ? nullptr : &it->second;}
const shader_config* render_pass_data::get_shader(const std::string& guid) const{auto it=shaders.find(guid);return it==shaders.end() ? nullptr : &it->second;}
sub_shader_config* render_pass_data::get_sub_shader(const std::string& guid){auto it=sub_shaders.find(guid);return it==sub_shaders.end() ? nullptr : &it->second;}
const sub_shader_config* render_pass_data::get_sub_shader(const std::string& guid) const{auto it=sub_shaders.find(guid);return it==sub_shaders.end() ? nullptr : &it->second;}
std::vector<const pass_config*> render_pass_data::get_passes(pass_stage stage) const{
    std::vector<const pass_config*> result;
    for(const auto& [guid,pass]:passes)if(pass.enabled && pass.stage==stage)result.push_back(&pass);
    std::sort(result.begin(),result.end(),[](const auto* a,const auto* b){return a->order==b->order ? a->guid<b->guid : a->order<b->order;});
    return result;
}
}
