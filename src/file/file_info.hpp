#ifndef __RENDER_FILE_INFO_HPP
#define __RENDER_FILE_INFO_HPP

#include <cstring>
#include <filesystem>
#include <tools/math/random.hpp>
#include <tools/structures/AABB.hpp>
#include <array>

namespace render{
    //文件类型
    enum class file_type{
        cpp,
        shader,//着色器源码
        render_pass,//.pass 配置
        shader_config,//.shader 组合配置
        sub_shader,//.sub_shader 阶段配置
        spirv,//shader编译文件
        model,
        scene,//场景配置文件
        object,//独立对象配置文件
        image,//图片文件，CPU 解码数据
        texture,//library 中的 BC DDS
        geometry,//library中的分页几何资源
        unknown
    };
    //文件信息
    struct file_info{
        std::string guid;
        std::string name;
        std::filesystem::path path; // 源资源相对于 source_root；派生资源相对于 library_root。
        file_type type = file_type::unknown;
        std::string source_guid;
        std::array<std::string,3> lod_guids;
        AABB bounds;
        uint32_t lod = 0;
        bool transparent = false;
        bool generated = false; // library/generated 下的派生资源，path 为绝对路径。
    };
}

#endif // __RENDER_FILE_INFO_HPP
