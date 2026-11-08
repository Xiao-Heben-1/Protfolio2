#ifndef __RENDER_TEXTURE_CONFIG_HPP
#define __RENDER_TEXTURE_CONFIG_HPP
#include <file/config/file_config.hpp>
#include <filesystem>

namespace render{
    // 格式可选 auto/bc1/bc3；auto 根据源图 alpha 选择 BC1 或 BC3。
    struct texture_config:public file_config{
        std::string format = "auto";
        bool srgb = true;
        bool generate_mips = true;
        bool alpha_weighted_mips = false; // 仅透明颜色贴图启用；遮罩打包纹理必须独立过滤 RGBA。
        bool flip_y = false;
        uint32_t width = 0;
        uint32_t height = 0;
        uint32_t mip_levels = 0;
        bool has_transparency = false; // 源图存在非 255 Alpha，用于选择 BC3；不代表材质采用透明渲染。
        std::string resolved_format;
        std::string signature;
        static texture_config load(const std::filesystem::path& path);
    private:
        friend class file_scan;
        void save(const std::filesystem::path& path,const std::string& source_path,const std::string& library_path) const;
    };
}
#endif
