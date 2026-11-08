#include <file/file_scan.hpp>
#include <algorithm>
#include <cctype>

namespace render{
    file_type file_scan::get_file_type(const std::filesystem::path& path) const noexcept{
        std::string extension = path.extension().string();
        std::transform(extension.begin(),extension.end(),extension.begin(),[](unsigned char c){return static_cast<char>(std::tolower(c));});

        if(extension == ".cpp" || extension == ".hpp" || extension == ".h"){
            return file_type::cpp;
        }
        else if(extension == ".vert" || extension == ".frag" || extension == ".geom" || extension == ".comp" || extension == ".tesc" || extension == ".tese" || extension == ".glsl" || extension == ".hlsl"){
            return file_type::shader;
        }
        else if(extension == ".spv"){
            return file_type::spirv;
        }
        else if(extension == ".fbx" || extension == ".obj" || extension == ".gltf" || extension == ".glb" ||
                extension == ".dae" || extension == ".ply" || extension == ".stl" || extension == ".3ds" || extension == ".blend" || extension == ".x" || extension == ".off"){
            return file_type::model;
        }
        else if(extension == ".scene"){
            return file_type::scene;
        }
        else if(extension == ".pass") return file_type::render_pass;
        else if(extension == ".shader") return file_type::shader_config;
        else if(extension == ".sub_shader") return file_type::sub_shader;
        else if(extension == ".object") return file_type::object;
        else if(extension == ".dds") return file_type::texture;
        else if(extension == ".png" || extension == ".jpg" || extension == ".jpeg" || extension == ".bmp" ||
                extension == ".tga" || extension == ".gif" || extension == ".psd" || extension == ".ppm" || extension == ".pgm"){
            return file_type::image;
        }
        return file_type::unknown;
    }
}
