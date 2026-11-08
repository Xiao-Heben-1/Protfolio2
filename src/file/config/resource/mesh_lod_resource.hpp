#pragma once
#include <file/file_info.hpp>
#include <core/component/mesh/mesh_data.hpp>
#include <file/config/resource/page_manager.hpp>

namespace render {
class file_scan;
struct mesh_lod_resource : file_info {
    std::string signature;
    uint64_t data_offset=0;
    uint64_t vertex_bytes=0;
    uint64_t index_bytes=0;
    std::vector<sub_mesh> sub_meshes;
    std::vector<page_descriptor> pages;
    static mesh_lod_resource load(const std::filesystem::path& file,uint32_t page_bytes=64*1024);
    void register_pages(page_manager& manager) const;
private:
    friend class file_scan;
    friend class mesh_stream_test;
    static mesh_lod_resource write(const mesh_data& data,const std::string& source_guid,
        uint32_t lod,bool transparent,const std::string& signature,const std::filesystem::path& directory,uint32_t page_bytes=64*1024);
};
}
