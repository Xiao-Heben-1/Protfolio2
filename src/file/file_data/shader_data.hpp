#ifndef __RENDER_SHADER_DATA_HPP
#define __RENDER_SHADER_DATA_HPP

#include <file/file_data/file_data.hpp>
namespace render{
    class file_resources;
    // Shader 源文件资源占位；源码读取和编译后续实现。
    // 编译后的 SPIR-V 目录及管线由 spirv_data 管理。
    class shader_data final:public file_data{
        friend class file_resources;
    private:
        explicit shader_data(VkDevice device):file_data(device){}
        ~shader_data() = default;
        void register_data(const std::string&) override{}
        void register_library(const std::string&) override{}
        void remove_data(const std::string&) override{}
        void cleanup() noexcept override{}
    };
}

#endif // __RENDER_SHADER_DATA_HPP
