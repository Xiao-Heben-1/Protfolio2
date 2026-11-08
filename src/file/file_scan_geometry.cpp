#include <file/file_scan.hpp>
#include <file/file_system.hpp>
#include <file/tools/library_cache.hpp>
#include <tools/model/QEM.hpp>

namespace render {
void file_scan::restore_geometry(const std::string& guid){
    auto& source=system.files.at(guid);
    const auto signature=library_cache::hash("qem-local-flipv-0.50-0.35/"+library_cache::hash_file(root_path/source.path));
    for(const auto& [key,info]:system.geometry_files){
        if(info.source_guid!=guid)continue;
        const auto resource=mesh_lod_resource::load(library_root/info.path);
        if(resource.signature!=signature)continue;
        source.lod_guids[resource.lod]=key;
        source.bounds=merge(source.bounds,resource.bounds);
        source.transparent=resource.transparent;
    }
}
void file_scan::register_geometry(const mesh_lod_resource& resource){
    file_info info=resource;info.path=resource.path.lexically_relative(library_root);
    system.geometry_files.insert_or_assign(info.guid,info);
    auto source=system.files.find(info.source_guid);
    if(source!=system.files.end()){
        source->second.lod_guids[info.lod]=info.guid;
        source->second.bounds=merge(source->second.bounds,info.bounds);
        source->second.transparent=info.transparent;
    }
}
std::array<mesh_lod_resource,3> file_scan::load_mesh_resources(const std::filesystem::path& source,bool transparent,
    const std::function<mesh_data()>& build,page_manager& pages){
    const auto absolute=(root_path/source).lexically_normal();
    const auto relative=in_library(absolute) ? absolute : absolute.lexically_relative(root_path);
    if(path_mp.find(relative.generic_string())==path_mp.end()){
        if(in_library(absolute))register_generated(absolute);else add_file(relative);
    }
    const auto source_guid=path_mp.at(relative.generic_string());
    const auto signature=library_cache::hash("qem-local-flipv-0.50-0.35/"+library_cache::hash_file(absolute));
    const auto directory=library_root/"geometry"/source_guid;
    std::array<mesh_lod_resource,3> result;
    if(std::filesystem::exists(directory))for(const auto& entry:std::filesystem::directory_iterator(directory)){
        if(entry.path().extension()!=".prim")continue;
        auto resource=mesh_lod_resource::load(entry.path());
        if(resource.signature==signature && resource.transparent==transparent)result[resource.lod]=std::move(resource);
    }
    if(result[0].guid.empty() || result[1].guid.empty() || result[2].guid.empty()){
        const auto original=build();
        if(result[0].guid.empty())result[0]=mesh_lod_resource::write(original,source_guid,0,transparent,signature,directory);
        for(uint32_t level=1;level<3;++level)if(result[level].guid.empty()){
            const auto simplified=QEM::lod_generater(original,level==1 ? .50f : .35f);
            result[level]=mesh_lod_resource::write(simplified,source_guid,level,transparent,signature,directory);
        }
    }
    system.files.at(source_guid).bounds=AABB();
    for(const auto& resource:result){register_geometry(resource);resource.register_pages(pages);}
    return result;
}
}
