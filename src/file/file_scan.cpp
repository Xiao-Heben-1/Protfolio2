#include <file/file_scan.hpp>
#include <file/file_system.hpp>
#include <file/tools/library_cache.hpp>
#include <file/tools/guid.hpp>
#include <rapidjson/document.h>
#include <fstream>
#include <file/tools/file_path.hpp>
#include <stdexcept>

namespace render{
    file_scan::file_scan(file_system& system):system(system){}
    file_scan::~file_scan(){stop_monitor();}

    bool file_scan::in_library(const std::filesystem::path& path) const{
        const auto absolute=path.lexically_normal();
        auto file=absolute.begin(),root=library_root.begin();
        for(;root!=library_root.end();++root,++file){
            if(file==absolute.end())return false;
            const auto& a=file->native();const auto& b=root->native();
            if(CompareStringOrdinal(a.data(),int(a.size()),b.data(),int(b.size()),TRUE)!=CSTR_EQUAL)return false;
        }
        return true;
    }

    void file_scan::init(const std::filesystem::path& path){
        library_root = system.config.library_root;
        start_monitor(path);
        for(auto type : {file_type::shader,file_type::spirv,file_type::scene,file_type::object,file_type::image,file_type::render_pass,file_type::shader_config,file_type::sub_shader})
            system.res.get_data(type)->scan = this;
        const auto& library = library_root;
        std::filesystem::create_directories(library);

        // 第一遍恢复缓存索引；路径、源关联及内容指纹只由 scan 持有。
        for(const auto& entry : std::filesystem::recursive_directory_iterator(library)){
            if(!entry.is_regular_file()) continue;
            const auto extension = entry.path().extension();
            if(extension==".prim" && entry.path().stem().string().size()==32){
                register_geometry(mesh_lod_resource::load(entry.path()));continue;
            }
            if(extension != ".texture_config" && extension != ".asset") continue;
            const auto bytes = library_cache::read(entry.path());
            rapidjson::Document json;
            json.Parse(reinterpret_cast<const char*>(bytes.data()),bytes.size());
            if(json.HasParseError() || !json.IsObject() || !json.HasMember("uid") || !json["uid"].IsString() ||
               !json.HasMember("source_path") || !json["source_path"].IsString() ||
               !json.HasMember("signature") || !json["signature"].IsString()) throw std::runtime_error("Invalid library metadata");
            const std::string uid = json["uid"].GetString();
            const auto cache = entry.path().parent_path() / entry.path().stem();
            const auto source_type=get_file_type(path_from_utf8(json["source_path"].GetString()));
            const auto type = extension == ".texture_config" ? file_type::texture :
                source_type==file_type::sub_shader ? file_type::sub_shader : file_type::spirv;
            library_files[uid] = {cache.lexically_relative(library_root),path_from_utf8(json["source_path"].GetString()),type,json["signature"].GetString()};
            if(type!=file_type::sub_shader)system.res.get_data(type)->register_library(uid);
        }

        // 生成目录独立建索引，绝不递归纳入 src，也不扫描 library 中的备份目录。
        const auto generated=library_root/"generated";
        if(std::filesystem::exists(generated))for(const auto& entry:std::filesystem::recursive_directory_iterator(generated))
            if(entry.is_regular_file() && entry.path().extension()!=".guid" && entry.path().extension()!=".signature")register_generated(entry.path());
        // 第二遍扫描源文件，移除失去源文件的缓存，再补建缺失或更新的缓存。
        scan();
        std::vector<std::string> removed;
        for(const auto& [uid,entry] : library_files){
            const auto source = system.files.find(uid);
            const bool matches = source != system.files.end() &&
                (entry.type == file_type::texture ? source->second.type == file_type::image :
                 entry.type == file_type::sub_shader ? source->second.type == file_type::sub_shader :
                 source->second.type == file_type::shader || source->second.type == file_type::spirv);
            if(!matches) removed.push_back(uid);
        }
        for(const auto& uid : removed) remove_library(uid);
        for(const auto& [uid,source] : system.files) import_source(uid);
        initialized = true;
        shaders_dirty = false;
    }

    std::filesystem::path file_scan::get_source_path(const std::string& uid) const{
        return root_path / system.files.at(uid).path;
    }
    std::filesystem::path file_scan::get_library_path(const std::string& uid) const{
        if(auto geometry=system.geometry_files.find(uid);geometry!=system.geometry_files.end())return library_root/geometry->second.path;
        return library_root / library_files.at(uid).path;
    }

    void file_scan::scan(const std::filesystem::path& path){
        if(in_library(root_path/path) || !std::filesystem::is_directory(root_path / path)) return;
        for(auto it = std::filesystem::recursive_directory_iterator(root_path / path); it != std::filesystem::recursive_directory_iterator(); ++it){
            const auto& entry = *it;
            const auto relative = entry.path().lexically_relative(root_path);
            if(in_library(entry.path())){
                it.disable_recursion_pending(); continue;
            }
            if(entry.is_regular_file()) add_file(relative);
        }
    }

    void file_scan::start_monitor(const std::filesystem::path& path){
        stop_monitor();
        root_path = std::filesystem::absolute(path).lexically_normal();
        directory_handle = CreateFileW(root_path.c_str(),FILE_LIST_DIRECTORY,
            FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,nullptr,OPEN_EXISTING,
            FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OVERLAPPED,nullptr);
        if(directory_handle == INVALID_HANDLE_VALUE) throw std::runtime_error("Cannot watch project directory");
        stop_event = CreateEventW(nullptr,TRUE,FALSE,nullptr);
        if(!stop_event){CloseHandle(directory_handle); directory_handle = INVALID_HANDLE_VALUE; throw std::runtime_error("Cannot create file watch stop event");}
        monitor_thread = std::thread(&file_scan::monitor_loop,this);
    }

    void file_scan::stop_monitor(){
        if(stop_event) SetEvent(stop_event);
        if(directory_handle != INVALID_HANDLE_VALUE) CancelIoEx(directory_handle,nullptr);
        if(monitor_thread.joinable()) monitor_thread.join();
        if(directory_handle != INVALID_HANDLE_VALUE){CloseHandle(directory_handle); directory_handle = INVALID_HANDLE_VALUE;}
        if(stop_event){CloseHandle(stop_event); stop_event = nullptr;}
    }

    void file_scan::monitor_loop(){
        alignas(FILE_NOTIFY_INFORMATION) BYTE buffer[65536];
        OVERLAPPED overlapped{};
        overlapped.hEvent = CreateEventW(nullptr,TRUE,FALSE,nullptr);
        if(!overlapped.hEvent) return;
        while(WaitForSingleObject(stop_event,0) != WAIT_OBJECT_0){
            ResetEvent(overlapped.hEvent);
            if(!ReadDirectoryChangesW(directory_handle,buffer,sizeof(buffer),TRUE,
                FILE_NOTIFY_CHANGE_FILE_NAME | FILE_NOTIFY_CHANGE_DIR_NAME | FILE_NOTIFY_CHANGE_LAST_WRITE | FILE_NOTIFY_CHANGE_SIZE,
                nullptr,&overlapped,nullptr)) break;
            HANDLE events[]{stop_event,overlapped.hEvent};
            const DWORD result = WaitForMultipleObjects(2,events,FALSE,INFINITE);
            DWORD bytes = 0;
            if(result != WAIT_OBJECT_0 + 1){
                CancelIoEx(directory_handle,&overlapped);
                GetOverlappedResult(directory_handle,&overlapped,&bytes,TRUE);
                break;
            }
            if(!GetOverlappedResult(directory_handle,&overlapped,&bytes,FALSE)) break;
            std::lock_guard lock(event_mutex);
            if(bytes == 0){rescan_pending = true; continue;}
            BYTE* current = buffer;
            while(true){
                const auto* info = reinterpret_cast<const FILE_NOTIFY_INFORMATION*>(current);
                const std::wstring name(info->FileName,info->FileNameLength / sizeof(wchar_t));
                pending_events.push_back({info->Action,name});
                if(info->NextEntryOffset == 0) break;
                current += info->NextEntryOffset;
            }
        }
        CloseHandle(overlapped.hEvent);
    }

    void file_scan::process_events(){
        std::vector<event> events;
        bool rescan = false;
        {
            std::lock_guard lock(event_mutex);
            events.swap(pending_events);
            rescan = rescan_pending;
            rescan_pending = false;
        }
        for(const auto& entry : events) handle_event(entry.action,entry.path);
        if(rescan){
            std::vector<std::filesystem::path> removed;
            for(const auto& [path,uid] : path_mp){
                if(!std::filesystem::exists(root_path / path)) removed.emplace_back(path);
            }
            for(const auto& path : removed) remove_file(path);
            scan();
        }
        if(shaders_dirty){
            for(const auto& [uid,source] : system.files)
                if(source.type == file_type::shader || source.type == file_type::spirv || source.type == file_type::sub_shader) import_spirv(uid);
            shaders_dirty = false;
        }
    }

    void file_scan::handle_event(DWORD action,const std::filesystem::path& path){
        // library 不属于源文件监听范围。
        if(in_library(root_path/path)){
            if(action == FILE_ACTION_RENAMED_NEW_NAME && !rename_old_path.empty()) remove_file(rename_old_path);
            if(action == FILE_ACTION_RENAMED_OLD_NAME || action == FILE_ACTION_RENAMED_NEW_NAME) rename_old_path.clear();
            return;
        }
        switch(action){
            case FILE_ACTION_ADDED:
            case FILE_ACTION_MODIFIED: add_file(path); break;
            case FILE_ACTION_REMOVED: remove_file(path); break;
            case FILE_ACTION_RENAMED_OLD_NAME: rename_old_path = path; break;
            case FILE_ACTION_RENAMED_NEW_NAME:
                if(rename_old_path.empty()) add_file(path);
                else move_file(rename_old_path,path);
                rename_old_path.clear();
                break;
            default: break;
        }
    }

    void file_scan::add_file(const std::filesystem::path& path){
        if(in_library(root_path/path))return;
        std::error_code error;
        if(std::filesystem::is_directory(root_path / path,error)){scan(path); return;}
        if(!std::filesystem::is_regular_file(root_path / path,error)) return;
        const auto type = get_file_type(path);
        if(path.extension()==".guid")return;
        const auto key = path.generic_string();
        const auto old = path_mp.find(key);
        const std::string uid = old == path_mp.end() ? load_guid(path) : old->second;
        file_info info{uid,path_to_utf8(path.filename()),path,type};
        if(initialized){
            const auto previous = system.files.find(uid);
            if(previous != system.files.end() && previous->second.type != type) remove_source(uid);
        }
        system.files[uid] = std::move(info);
        if(type==file_type::model)restore_geometry(uid);
        if(initialized) import_source(uid);
        path_mp[key] = uid;
    }

    void file_scan::remove_file(const std::filesystem::path& path){
        const auto key = path.generic_string();
        for(auto it = path_mp.begin(); it != path_mp.end();){
            if(it->first != key && !it->first.starts_with(key + '/')){++it; continue;}
            remove_source(it->second);
            std::error_code error;
            std::filesystem::remove(root_path / (it->first + ".guid"),error);
            it = path_mp.erase(it);
        }
    }

    void file_scan::move_file(const std::filesystem::path& old_path,const std::filesystem::path& new_path){
        const auto old_key = old_path.generic_string();
        std::vector<std::pair<std::string,std::string>> moved;
        for(auto it = path_mp.begin(); it != path_mp.end();){
            if(it->first != old_key && !it->first.starts_with(old_key + '/')){++it; continue;}
            moved.push_back(*it);
            const auto target = it->first == old_key ? new_path : new_path / it->first.substr(old_key.size() + 1);
            if(target.extension()==".guid" || !std::filesystem::is_regular_file(root_path / target)){
                remove_source(it->second);
            }
            it = path_mp.erase(it);
        }
        for(const auto& [old_file,uid] : moved){
            const auto target = old_file == old_key ? new_path : new_path / old_file.substr(old_key.size() + 1);
            if(!std::filesystem::is_regular_file(root_path / target)) continue;
            const auto type = get_file_type(target);
            if(target.extension()==".guid")continue;
            auto old_guid = root_path / (old_file + ".guid");
            auto new_guid = root_path / target;
            new_guid += ".guid";
            // 目录移动时 sidecar 已随目录移动；单文件移动时补迁移。
            if(std::filesystem::exists(old_guid) && !std::filesystem::exists(new_guid)){
                std::filesystem::rename(old_guid,new_guid);
            }
            if(!std::filesystem::exists(new_guid)){std::ofstream file(new_guid); file << uid;}
            path_mp[target.generic_string()] = uid;
            add_file(target);
        }
        // 新移入的目录还可能包含尚未被索引的文件。
        add_file(new_path);
    }

    std::string file_scan::load_guid(const std::filesystem::path& path){
        const auto type=get_file_type(path);
        if(type==file_type::scene || type==file_type::object || type==file_type::render_pass || type==file_type::shader_config || type==file_type::sub_shader){
            std::ifstream input(root_path/path,std::ios::binary);
            input>>std::ws;
            if(input.peek()!='{'){
                std::string guid;std::getline(input,guid);if(!guid.empty() && guid.back()=='\r')guid.pop_back();
                library_cache::write_uid(root_path/path,guid);
                return guid;
            }
        }
        auto guid_path = root_path / path;
        guid_path += ".guid";
        std::ifstream file(guid_path);
        std::string uid;
        if(file >> uid) return uid;
        if(type==file_type::scene || type==file_type::object){
            const auto guid=type==file_type::scene ? scene_config::load(root_path/path).guid : object_config::load(root_path/path).guid;
            library_cache::write_uid(root_path/path,guid);
            return guid;
        }
        return create_guid(path);
    }

    std::string file_scan::create_guid(const std::filesystem::path& path){
        std::string uid = generate_guid();
        auto guid_path = root_path / path;
        guid_path += ".guid";
        std::ofstream file(guid_path);
        if(!(file << uid)) throw std::runtime_error("Cannot persist file UID: " + path.string());
        return uid;
    }
}
