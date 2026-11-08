#ifndef __RENDER_BC_TEXTURE_HPP
#define __RENDER_BC_TEXTURE_HPP
#include <cstdint>
#include <vector>

namespace render{
struct bc_mip{uint32_t width,height; std::vector<uint8_t> blocks;};
/// @brief 将RGBA8纹理压缩为BC1/BC3，并可生成mipmap链。
/// @param rgba 原始RGBA8像素数据。
/// @param width 纹理宽度。
/// @param height 纹理高度。
/// @param is_alpha 是否使用alpha通道；false为BC1，true为BC3。
/// @param is_srgb 是否按sRGB颜色空间处理mipmap。
/// @param is_mipmaps 是否生成完整mipmap链。
/// @param alpha_weighted_mips 仅 Alpha 表示不透明度时可启用；否则颜色与 Alpha 独立缩小。
/// @return 各级mipmap的BC压缩数据。
std::vector<bc_mip> compress_bc(std::vector<uint8_t> rgba,uint32_t width,uint32_t height,bool is_alpha,bool is_srgb,bool is_mipmaps,bool alpha_weighted_mips=false);

/// @brief 将BC1/BC3压缩后的mipmap链封装为DDS文件二进制数据。
/// @param mip_levels BC压缩后的mipmap链。
/// @param is_alpha 是否使用alpha通道；false对应BC1，true对应BC3。
/// @param is_srgb 是否使用sRGB格式。
/// @return 完整DDS文件的二进制数据。
std::vector<uint8_t> make_dds_bytes(const std::vector<bc_mip>& mip_levels,bool is_alpha,bool is_srgb);
}
#endif
