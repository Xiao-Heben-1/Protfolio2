#include <file/file_scan.hpp>
#include <file/file_system.hpp>
#include <file/tools/library_cache.hpp>
#include <file/tools/bc_texture.hpp>
#include <rapidjson/prettywriter.h>
#include <rapidjson/stringbuffer.h>
#define STB_IMAGE_STATIC
#define STB_IMAGE_IMPLEMENTATION
#define STBI_NO_STDIO
#include <assimp/contrib/stb/stb_image.h>
#include <algorithm>
#include <fstream>
#include <file/tools/file_path.hpp>
#include <limits>
#include <memory>
#include <regex>
#include <set>
#include <stdexcept>

namespace render{
namespace{
    // 根据配置判断已有 DDS 是否完整，决定是否需要重新生成。
    bool valid_dds(const std::filesystem::path& path,const texture_config& config){
        if(!std::filesystem::is_regular_file(path) || !config.width || !config.height || !config.mip_levels) return false;
        // DDS + DX10 文件头共 148 字节；每个 4×4 块占 BC1 的 8 字节或 BC3 的 16 字节。
        uint64_t expected = 148; uint32_t width = config.width, height = config.height;
        for(uint32_t i = 0; i < config.mip_levels; ++i){
            expected += uint64_t((width + 3) / 4) * ((height + 3) / 4) * (config.resolved_format == "bc3" ? 16 : 8);
            width = std::max(1u,width / 2); height = std::max(1u,height / 2);
        }
        if(std::filesystem::file_size(path) != expected) return false;
        std::ifstream input(path,std::ios::binary); uint32_t magic = 0; input.read(reinterpret_cast<char*>(&magic),4);
        return magic == 0x20534444;
    }
    std::string stage_name(std::filesystem::path path){
        if(path.extension() == ".spv" || path.extension() == ".hlsl") path = path.stem();
        auto ext = path.extension().string();
        if(ext == ".vert" || ext == ".frag" || ext == ".geom" || ext == ".tesc" || ext == ".tese" || ext == ".comp") return ext.substr(1);
        return {};
    }
    std::string shader_fingerprint(const std::filesystem::path& path,const std::vector<std::filesystem::path>& includes,std::set<std::filesystem::path>& visited){
        auto normalized = std::filesystem::weakly_canonical(path);
        if(!visited.insert(normalized).second) return {};
        const auto bytes = library_cache::read(normalized);
        std::string source(bytes.begin(),bytes.end()), dependencies;
        static const std::regex directive(R"include(#\s*include\s*["<]([^">]+)[">])include");
        for(std::sregex_iterator it(source.begin(),source.end(),directive),end; it != end; ++it){
            auto dependency = path.parent_path() / (*it)[1].str();
            if(!std::filesystem::exists(dependency))for(const auto& include:includes){
                const auto candidate=include/(*it)[1].str();
                if(std::filesystem::exists(candidate)){dependency=candidate;break;}
            }
            dependencies += shader_fingerprint(dependency,includes,visited);
        }
        return library_cache::hash(source + dependencies);
    }
    std::wstring quote(const std::wstring& value){
        std::wstring result = L"\""; size_t slashes = 0;
        for(wchar_t c : value){
            if(c == L'\\'){++slashes; continue;}
            if(c == L'"'){result.append(slashes * 2 + 1,L'\\'); result += c;}
            else{result.append(slashes,L'\\'); result += c;}
            slashes = 0;
        }
        result.append(slashes * 2,L'\\'); result += L'"'; return result;
    }
    void compile(const std::filesystem::path& compiler,const std::filesystem::path& source,const std::filesystem::path& target,
        const std::vector<std::filesystem::path>& includes,const std::string& stage,const sub_shader_config& config){
        const auto wide=[](const std::string& text){return path_from_utf8(text).wstring();};
        std::wstring command = quote(compiler.wstring()) + L" --target-env=vulkan1.3 -fshader-stage=" + wide(stage);
        command+=L" "+quote(L"-fentry-point="+wide(config.entry_point));
        for(const auto& include:includes)command+=L" -I "+quote(include.wstring());
        for(const auto& [name,value]:config.defines)command+=L" "+quote(L"-D"+wide(name)+(value.empty() ? L"" : L"="+wide(value)));
        command+=L" "+quote(source.wstring())+L" -o "+quote(target.wstring());
        STARTUPINFOW startup{}; startup.cb = sizeof(startup);
        PROCESS_INFORMATION process{};
        if(!CreateProcessW(nullptr,command.data(),nullptr,nullptr,FALSE,CREATE_NO_WINDOW,nullptr,nullptr,&startup,&process)) throw std::runtime_error("Cannot start glslc");
        WaitForSingleObject(process.hProcess,INFINITE);
        DWORD status = 1; GetExitCodeProcess(process.hProcess,&status);
        CloseHandle(process.hThread); CloseHandle(process.hProcess);
        if(status != 0) throw std::runtime_error("Shader compilation failed: " + source.string());
    }

}
    void file_scan::import_source(const std::string& uid){
        const auto type = system.files.at(uid).type;
        if(auto* data = system.res.get_data(type)) data->register_data(uid);
        if(type==file_type::object){
            const auto& config=*system.res.get_object_data().get_object(uid);
            auto& info=system.files.at(uid);
            info.transparent=config.is_transparent;
            if(config.mesh){
                info.source_guid=config.mesh->source_guid;
                info.lod_guids=config.mesh->lod_guids;
                if(config.mesh->bounds)info.bounds=*config.mesh->bounds;
            }
        }
        if(type == file_type::image) import_image(uid);
        else if(type == file_type::shader || type == file_type::spirv || type == file_type::sub_shader){
            import_spirv(uid);
            if(type == file_type::shader || type == file_type::sub_shader || type == file_type::spirv) shaders_dirty = true;
        }
    }
    void file_scan::remove_library(const std::string& uid){
        const auto it = library_files.find(uid);
        if(it == library_files.end()) return;
        if(it->second.type!=file_type::sub_shader)system.res.get_data(it->second.type)->remove_data(uid);//只清理编译产物的内存记录
        const auto cache = library_root / it->second.path;
        const auto library = library_root;
        library_cache::remove(cache,library);
        library_cache::remove(cache.string() + (it->second.type == file_type::texture ? ".texture_config" : ".asset"),library);
        library_cache::remove(cache.string() + ".guid",library);
        library_files.erase(it);
    }
    void file_scan::remove_source(const std::string& uid){
        const auto it = system.files.find(uid);
        if(it == system.files.end()) return;
        const auto type = it->second.type;
        remove_library(uid);
        for(auto geometry=system.geometry_files.begin();geometry!=system.geometry_files.end();){
            if(geometry->second.source_guid!=uid){++geometry;continue;}
            library_cache::remove(library_root/geometry->second.path,library_root);
            geometry=system.geometry_files.erase(geometry);
        }
        if(auto* data = system.res.get_data(type)) data->remove_data(uid);
        if(type == file_type::shader || type == file_type::sub_shader || type == file_type::spirv) shaders_dirty = true;
        system.files.erase(it);
    }
    void file_scan::import_image(const std::string& uid){
        const auto& source_info = system.files.at(uid);
        auto info = source_info;
        info.path = get_source_path(uid);
        const auto library_path = library_root;
        const auto previous = library_files.find(uid);
        library_cache::validate_uid(uid);
        const auto cache = library_path / "textures" / (uid + ".dds");
        const auto meta = cache.string() + ".texture_config";
        texture_config config;
        if(std::filesystem::exists(meta)) config = texture_config::load(meta);
        if(config.format != "auto" && config.format != "bc1" && config.format != "bc3") throw std::runtime_error("Unsupported BC texture format");
        auto encoded = library_cache::read(info.path);
        // 源图片内容和压缩选项共同决定指纹；任一变化都需要重建 DDS。
        const auto signature = library_cache::hash(std::string(encoded.begin(),encoded.end()) + "bc-v2/" + config.format +
            (config.srgb ? "/srgb" : "/linear") + (config.generate_mips ? "/mips" : "/single") + (config.flip_y ? "/flip" : "/keep") +
            (config.alpha_weighted_mips ? "/alpha_weighted" : "/independent"));
        bool changed = config.signature != signature || !valid_dds(cache,config);
        if(changed){
            if(encoded.empty() || encoded.size() > size_t(std::numeric_limits<int>::max())) throw std::runtime_error("Image size is unsupported");
            int width = 0,height = 0,channels = 0;
            // 统一解码成 RGBA；unique_ptr 离开作用域时用 stbi_image_free 释放像素内存。
            std::unique_ptr<stbi_uc,decltype(&stbi_image_free)> decoded(
                stbi_load_from_memory(encoded.data(),int(encoded.size()),&width,&height,&channels,STBI_rgb_alpha),stbi_image_free);
            if(!decoded) throw std::runtime_error("Cannot decode image: " + info.path.string());
            std::vector<uint8_t> rgba(decoded.get(),decoded.get() + size_t(width) * height * 4);
            if(config.flip_y){
                for(int y = 0; y < height / 2; ++y) for(size_t x = 0; x < size_t(width) * 4; ++x)
                    std::swap(rgba[size_t(y) * width * 4 + x],rgba[size_t(height - y - 1) * width * 4 + x]);
            }
            // auto 模式：Alpha 全为 255 用 BC1，否则用 BC3 保存 Alpha（也可能是材质遮罩）。
            config.has_transparency = false;
            for(size_t i = 3; i < rgba.size(); i += 4) if(rgba[i] != 255){config.has_transparency = true; break;}
            if(config.format == "bc1" && config.has_transparency) throw std::runtime_error("Use BC3 to preserve texture alpha");
            const bool alpha = config.format == "bc3" || config.has_transparency;
            // 按配置生成 mip 链并压缩，再封装成带 DX10 头的 DDS 文件。
            auto mips = compress_bc(std::move(rgba),uint32_t(width),uint32_t(height),alpha,config.srgb,config.generate_mips,config.alpha_weighted_mips);
            auto dds = make_dds_bytes(mips,alpha,config.srgb);
            std::filesystem::create_directories(cache.parent_path());
            auto temporary = cache; temporary += ".tmp";
            {std::ofstream output(temporary,std::ios::binary); if(!output.write(reinterpret_cast<const char*>(dds.data()),dds.size())) throw std::runtime_error("Cannot write BC texture cache");}
            library_cache::commit(temporary,cache);
            config.width = width; config.height = height; config.mip_levels = static_cast<uint32_t>(mips.size());
            config.resolved_format = alpha ? "bc3" : "bc1"; config.signature = signature;
        }
        // 配置写入项目相对路径；源文件改名或移动时，即使像素不变也更新路径。
        const auto source = path_to_utf8(source_info.path);
        const auto destination = path_to_utf8(cache.lexically_relative(library_root));
        const bool moved = config.guid != uid || config.name != info.name || previous == library_files.end() ||
            previous->second.source_path != source_info.path || previous->second.path != cache.lexically_relative(library_root);
        config.guid = uid; config.name = info.name;
        if(changed || moved) config.save(meta,source,destination);
        if(previous != library_files.end() && previous->second.path != cache.lexically_relative(library_root)) remove_library(uid);
        library_cache::write_uid(cache,uid);
        library_files[uid] = {cache.lexically_relative(library_root),source_info.path,file_type::texture,signature};
        if(changed || moved || !system.res.get_image_data().get_entries().contains(uid))
            system.res.get_data(file_type::texture)->register_library(uid);
    }

    void file_scan::import_spirv(const std::string& uid){
        const auto& source = system.files.at(uid);
        auto path = get_source_path(uid);
        auto stage = stage_name(path);
        sub_shader_config config;
        auto includes=system.config.shader_include_roots;
        bool precompiled=source.type==file_type::spirv;
        if(source.type==file_type::sub_shader){
            config=sub_shader_config::load(path);
            if(!config.builtin_source.empty()){
                const auto engine_source=system.config.engine_root.empty() ? system.config.builtin_shader_root.parent_path()/"shader" : system.config.engine_root/"shader";
                path=engine_source/config.builtin_source;
                includes.insert(includes.begin(),{engine_source/"core/include",engine_source});
            }else{
                const auto input=system.files.find(config.source_guid);
                if(input==system.files.end()){remove_library(uid);return;}
                path=get_source_path(config.source_guid);
                precompiled=input->second.type==file_type::spirv;
            }
            constexpr const char* names[]={"vert","frag","geom","tesc","tese","comp"};
            stage=names[size_t(config.stage)];
            std::vector<std::filesystem::path> local;
            for(const auto& include:config.include_roots)local.push_back((root_path/include).lexically_normal());
            includes.insert(includes.begin(),local.begin(),local.end());
        }
        if(stage.empty()) return;//共享 include 的变化通过各入口的内容指纹处理
        library_cache::validate_uid(uid);
        auto stem = source.path;
        if(stem.extension() == ".spv" || stem.extension() == ".hlsl") stem = stem.parent_path() / stem.stem();
        stem = stem.parent_path() / stem.stem();
        const auto group = library_cache::hash(stem.generic_string());
        const auto old = library_files.find(uid);
        const auto relative = source.type==file_type::sub_shader ?
            std::filesystem::path("spirv")/"configured"/uid/(uid+"."+stage+".spv") :
            old != library_files.end() && old->second.source_path == source.path ? old->second.path :
            std::filesystem::path("spirv") / "imported" / group / (uid + "." + stage + ".spv");
        const auto cache = library_root / relative;
        std::set<std::filesystem::path> visited;
        std::string options=stage+"/"+config.entry_point+"/"+system.config.shader_compiler.generic_string();
        for(const auto& [key,value]:config.defines)options+="\n"+key+"="+value;
        for(const auto& include:includes)options+="\n"+include.generic_string();
        const auto signature = library_cache::hash("glslc-vulkan1.3-v2/"+options+
            (precompiled ? library_cache::hash_file(path) : shader_fingerprint(path,includes,visited)));
        const bool changed = old == library_files.end() || old->second.signature != signature ||
            old->second.path != relative || !std::filesystem::exists(cache);
        const bool moved = old == library_files.end() || old->second.source_path != source.path;
        if(changed){
            std::filesystem::create_directories(cache.parent_path());
            auto temporary = cache; temporary += ".tmp";
            if(precompiled) std::filesystem::copy_file(path,temporary,std::filesystem::copy_options::overwrite_existing);
            else compile(system.config.shader_compiler,path,temporary,includes,stage,config);
            library_cache::commit(temporary,cache);
            if(old != library_files.end() && old->second.path != relative) remove_library(uid);
        }
        library_files[uid] = {relative,source.path,source.type==file_type::sub_shader ? file_type::sub_shader : file_type::spirv,signature};
        if(changed || moved){
            rapidjson::StringBuffer buffer;
            rapidjson::PrettyWriter<rapidjson::StringBuffer> json(buffer);
            json.StartObject();
            auto string = [&](const char* key,const std::string& value){json.Key(key); json.String(value.data(),static_cast<rapidjson::SizeType>(value.size()));};
            string("uid",uid); string("source_path",path_to_utf8(source.path));
            string("library_path",path_to_utf8(relative)); string("signature",signature);
            json.EndObject();
            const auto meta = cache.string() + ".asset";
            const auto temporary = meta + ".tmp";
            {std::ofstream output(temporary,std::ios::binary); if(!output.write(buffer.GetString(),buffer.GetSize())) throw std::runtime_error("Cannot save SPIR-V metadata");}
            library_cache::commit(temporary,meta);
        }
        library_cache::write_uid(cache,uid);
        if(source.type!=file_type::sub_shader && (changed || moved || !system.res.get_spirv_data().get_entries().contains(uid)))
            system.res.get_data(file_type::spirv)->register_library(uid);
    }

    std::string file_scan::create_file(const std::filesystem::path& path,std::span<const uint8_t> bytes){
        const auto relative = path.is_absolute() ? path.lexically_relative(root_path) : path;
        const auto target = root_path / relative;
        std::filesystem::create_directories(target.parent_path());
        {std::ofstream output(target,std::ios::binary);
         if(!output.write(reinterpret_cast<const char*>(bytes.data()),bytes.size())) throw std::runtime_error("Cannot write source file");}
        if(in_library(target.lexically_normal())){
            const auto generated_path=target.lexically_normal().lexically_relative(library_root);
            if(*generated_path.begin()!=L"generated")return {};
            register_generated(target);return path_mp.at(target.lexically_normal().generic_string());
        }
        add_file(relative);
        const auto it = path_mp.find(relative.generic_string());
        return it == path_mp.end() ? std::string{} : it->second;
    }
    void file_scan::delete_file(const std::string& uid){
        const auto path = system.files.at(uid).path;
        std::filesystem::remove(root_path / path);
        remove_file(path);
    }
    void file_scan::set_texture_config(const std::string& uid,texture_config config){
        const auto& source = system.files.at(uid);
        const auto& cache = library_files.at(uid);
        config.guid = uid;
        config.name = source.name;
        config.signature.clear();
        config.save(get_library_path(uid).string() + ".texture_config",path_to_utf8(source.path),path_to_utf8(cache.path));
        import_image(uid);
    }
}

