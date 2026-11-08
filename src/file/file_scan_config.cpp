#include <file/file_scan.hpp>
#include <file/file_system.hpp>
#include <file/tools/library_cache.hpp>
#include <file/tools/file_path.hpp>
#include <fstream>
namespace render {
    void file_scan::register_generated(const std::filesystem::path& path){
        const auto absolute=std::filesystem::absolute(path).lexically_normal();const auto guid=load_guid(absolute);
        file_info info{guid,path_to_utf8(absolute.filename()),absolute,get_file_type(absolute)};info.generated=true;
        system.files.insert_or_assign(guid,std::move(info));path_mp[absolute.generic_string()]=guid;
        if(system.files.at(guid).type==file_type::model)restore_geometry(guid);
        if(initialized)import_source(guid);
    }
    void file_scan::generate_system_config(){
        // engine/config 是定义；workspace/config 是用户覆盖；library/generated 是输出。
        const auto materialize=[&](const std::filesystem::path& directory,bool overwrite){
            if(!std::filesystem::exists(directory))return;
            for(const auto& entry:std::filesystem::recursive_directory_iterator(directory)){
                if(!entry.is_regular_file() || entry.path().extension()==".guid")continue;
                const auto type=get_file_type(entry.path());
                if(type!=file_type::render_pass && type!=file_type::shader_config && type!=file_type::sub_shader && type!=file_type::scene && type!=file_type::object)continue;
                const auto target=library_root/"generated"/entry.path().lexically_relative(directory);
                const std::filesystem::path signature=target.wstring()+L".signature";
                const auto hash=library_cache::hash_file(entry.path());std::string previous;{std::ifstream in(signature);in>>previous;}
                if(!std::filesystem::exists(target) || overwrite || previous!=hash){
                    std::filesystem::create_directories(target.parent_path());
                    if(!std::filesystem::exists(target) || library_cache::hash_file(target)!=hash)
                        std::filesystem::copy_file(entry.path(),target,std::filesystem::copy_options::overwrite_existing);
                    std::ofstream(signature)<<hash;
                }
                register_generated(target);
            }
        };
        if(!system.config.engine_root.empty())materialize(system.config.engine_root/"config/system",false);
        materialize(system.config.workspace_root/"config",true);
    }
    void file_scan::save_project_config(const std::string& guid){
        const auto& info=system.files.at(guid);if(!info.generated)return;
        const auto relative=info.path.lexically_relative(library_root/"generated");
        const auto target=system.config.workspace_root/"config"/relative;
        std::filesystem::create_directories(target.parent_path());
        std::filesystem::copy_file(info.path,target,std::filesystem::copy_options::overwrite_existing);
    }
    void file_scan::register_config(const std::filesystem::path& relative,const std::string& guid){
        const auto absolute=(root_path/relative).lexically_normal();
        if(in_library(absolute)){register_generated(absolute);return;}
        library_cache::write_uid(root_path/relative,guid);
        const auto previous=path_mp.find(relative.generic_string());
        if(previous!=path_mp.end() && previous->second!=guid)remove_source(previous->second);
        path_mp[relative.generic_string()]=guid;
        add_file(relative);
    }
    void file_scan::save_pass(const std::filesystem::path& path,const pass_config& config){
        const auto relative=path.is_absolute() ? path.lexically_relative(root_path) : path;
        std::filesystem::create_directories((root_path/relative).parent_path());
        config.save(root_path/relative);register_config(relative,config.guid);
    }
    void file_scan::save_shader(const std::filesystem::path& path,const shader_config& config){
        const auto relative=path.is_absolute() ? path.lexically_relative(root_path) : path;
        std::filesystem::create_directories((root_path/relative).parent_path());
        config.save(root_path/relative);register_config(relative,config.guid);
    }
    void file_scan::save_sub_shader(const std::filesystem::path& path,const sub_shader_config& config){
        const auto relative=path.is_absolute() ? path.lexically_relative(root_path) : path;
        std::filesystem::create_directories((root_path/relative).parent_path());
        config.save(root_path/relative);register_config(relative,config.guid);
    }
    void file_scan::save_scene(const std::string& uid){
        const auto& config = *system.res.get_scene_data().get_scene(uid);
        save_scene(uid,get_source_path(uid),config);
    }
    void file_scan::save_scene(const std::string& uid,const std::filesystem::path& path,const scene_config& config){
        const auto relative = path.is_absolute() ? path.lexically_relative(root_path) : path;
        const auto target = root_path / relative;
        auto saved = config;
        saved.guid = uid;
        std::filesystem::create_directories(target.parent_path());
        saved.save(target);
        register_config(relative,uid);
    }
    void file_scan::save_object(const std::string& uid){
        const auto& config = *system.res.get_object_data().get_object(uid);
        save_object(uid,get_source_path(uid),config);
    }
    void file_scan::save_object(const std::string& uid,const std::filesystem::path& path,const object_config& config){
        const auto relative = path.is_absolute() ? path.lexically_relative(root_path) : path;
        const auto target = root_path / relative;
        auto saved = config;
        saved.guid = uid;
        std::filesystem::create_directories(target.parent_path());
        saved.save(target);
        register_config(relative,uid);
    }
}

