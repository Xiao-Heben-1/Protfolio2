#include <file/tools/bc_texture.hpp>
#include <algorithm>
#include <array>
#include <cmath>
#include <stdexcept>

namespace render{
namespace{
    /// @brief 将整数按小端序追加到字节数组。
    /// @param bytes 目标字节数组。
    /// @param value 需要写入的整数值。
    /// @param count 写入的字节数，从最低有效字节开始。
    void append(std::vector<uint8_t>& bytes,uint64_t value,uint32_t count){
        for(uint32_t i = 0; i < count; ++i) bytes.push_back(static_cast<uint8_t>(value >> (8 * i)));
    }

    /// @brief 将RGB888颜色量化并打包为RGB565格式。
    /// @param rgb RGB颜色，每个通道范围为0~255。
    /// @return 16位RGB565颜色，高5位为R，中6位为G，低5位为B。
    uint16_t rgb565(const std::array<int,3>& rgb){
        return uint16_t(((rgb[0] * 31 + 127) / 255 << 11) | ((rgb[1] * 63 + 127) / 255 << 5) | ((rgb[2] * 31 + 127) / 255));
    }

    /// @brief 将RGB565颜色解码回近似RGB888颜色。
    /// @param value 16位RGB565颜色。
    /// @return RGB颜色，每个通道恢复到0~255范围。
    std::array<int,3> unpack_rgb565(uint16_t value){
        int r = value >> 11, g = (value >> 5) & 63, b = value & 31;
        return {(r << 3) | (r >> 2),(g << 2) | (g >> 4),(b << 3) | (b >> 2)};
    }
    /// @brief 将一个4x4 RGBA8像素块压缩为BC1或BC3 block。
    /// @param pixels 4x4像素块，共16个RGBA8像素。
    /// @param is_alpha 是否压缩alpha通道；false生成BC1，true生成BC3。
    /// @param result 压缩结果追加到该字节数组。
    void block(const std::array<std::array<uint8_t,4>,16>& pixels,bool is_alpha,std::vector<uint8_t>& result){
        if(is_alpha){
            // BC3 alpha：寻找两个alpha端点。
            int lo = 255, hi = 0;
            for(const auto& p : pixels){
                lo = std::min(lo,int(p[3]));
                hi = std::max(hi,int(p[3]));
            }

            // 强制alpha0 > alpha1，使用8-alpha插值模式。
            if(hi == lo){
                if(hi < 255) ++hi;
                else --lo;
            }

            // 两个端点 + 六个等距插值alpha。
            const int palette[8]{
                hi,
                lo,
                (6 * hi + lo) / 7,
                (5 * hi + 2 * lo) / 7,
                (4 * hi + 3 * lo) / 7,
                (3 * hi + 4 * lo) / 7,
                (2 * hi + 5 * lo) / 7,
                (hi + 6 * lo) / 7
            };

            // 每个像素选择最接近的alpha候选，索引占3bit。
            uint64_t indices = 0;
            for(uint32_t p = 0; p < 16; ++p){
                const int value = int(pixels[p][3]);
                int best = 0;
                int error = std::abs(value - palette[0]);

                int candidate = std::abs(value - palette[1]);
                if(candidate < error){best = 1; error = candidate;}

                candidate = std::abs(value - palette[2]);
                if(candidate < error){best = 2; error = candidate;}

                candidate = std::abs(value - palette[3]);
                if(candidate < error){best = 3; error = candidate;}

                candidate = std::abs(value - palette[4]);
                if(candidate < error){best = 4; error = candidate;}

                candidate = std::abs(value - palette[5]);
                if(candidate < error){best = 5; error = candidate;}

                candidate = std::abs(value - palette[6]);
                if(candidate < error){best = 6; error = candidate;}

                candidate = std::abs(value - palette[7]);
                if(candidate < error) best = 7;

                indices |= uint64_t(best) << (3 * p);
            }

            // BC3 alpha块：2字节端点 + 48bit索引。
            append(result,hi,1);
            append(result,lo,1);
            append(result,indices,6);
        }

        // BC1 RGB：寻找三个通道各自的最小/最大值。
        std::array<int,3> lo{255,255,255}, hi{0,0,0};
        for(const auto& p : pixels){
            lo[0] = std::min(lo[0],int(p[0]));
            lo[1] = std::min(lo[1],int(p[1]));
            lo[2] = std::min(lo[2],int(p[2]));

            hi[0] = std::max(hi[0],int(p[0]));
            hi[1] = std::max(hi[1],int(p[1]));
            hi[2] = std::max(hi[2],int(p[2]));
        }

        // 两个RGB565颜色端点。
        uint16_t high = rgb565(hi), low = rgb565(lo);
        if(high == low){
            if(high < 65535) ++high;
            else --low;
        }

        const std::array<int,3> color_0 = unpack_rgb565(high);
        const std::array<int,3> color_1 = unpack_rgb565(low);

        // 4-color模式的两个插值颜色。
        const std::array<int,3> color_2{
            (2 * color_0[0] + color_1[0]) / 3,
            (2 * color_0[1] + color_1[1]) / 3,
            (2 * color_0[2] + color_1[2]) / 3
        };

        const std::array<int,3> color_3{
            (color_0[0] + 2 * color_1[0]) / 3,
            (color_0[1] + 2 * color_1[1]) / 3,
            (color_0[2] + 2 * color_1[2]) / 3
        };

        // 每个像素选择平方误差最小的颜色，索引占2bit。
        uint32_t indices = 0;
        for(uint32_t p = 0; p < 16; ++p){
            const int r = int(pixels[p][0]);
            const int g = int(pixels[p][1]);
            const int b = int(pixels[p][2]);

            int dr = r - color_0[0], dg = g - color_0[1], db = b - color_0[2];
            int best = 0;
            int error = dr * dr + dg * dg + db * db;

            dr = r - color_1[0]; dg = g - color_1[1]; db = b - color_1[2];
            int candidate = dr * dr + dg * dg + db * db;
            if(candidate < error){best = 1; error = candidate;}

            dr = r - color_2[0]; dg = g - color_2[1]; db = b - color_2[2];
            candidate = dr * dr + dg * dg + db * db;
            if(candidate < error){best = 2; error = candidate;}

            dr = r - color_3[0]; dg = g - color_3[1]; db = b - color_3[2];
            candidate = dr * dr + dg * dg + db * db;
            if(candidate < error) best = 3;

            indices |= uint32_t(best) << (2 * p);
        }

        // BC1颜色块：两个RGB565端点 + 32bit索引。
        append(result,high,2);
        append(result,low,2);
        append(result,indices,4);
    }
    /// @brief 将sRGB编码值转换为线性RGB值。
    /// @param value sRGB通道值，范围0~1。
    /// @return 对应的线性RGB值。
    float srgb_to_linear(float value){
        return value <= 0.04045f ? value / 12.92f : std::pow((value + 0.055f) / 1.055f,2.4f);
    }

    /// @brief 将线性RGB值转换为sRGB编码值。
    /// @param value 线性RGB通道值，范围0~1。
    /// @return 对应的sRGB编码值。
    float linear_to_srgb(float value){
        return value <= 0.0031308f ? value * 12.92f : 1.055f * std::pow(value,1.0f / 2.4f) - 0.055f;
    }
}
    /// @brief 将RGBA8纹理生成BC1/BC3压缩的mipmap链。
    /// @param rgba_pixels 原始RGBA8像素数据。
    /// @param width 原始纹理宽度。
    /// @param height 原始纹理高度。
    /// @param is_alpha 是否使用alpha通道；false生成BC1，true生成BC3。
    /// @param is_srgb 是否按sRGB颜色空间生成mipmap。
    /// @param is_mipmaps 是否生成完整mipmap链。
    /// @return BC压缩后的mipmap链。
    std::vector<bc_mip> compress_bc(std::vector<uint8_t> rgba_pixels,uint32_t width,uint32_t height,bool is_alpha,bool is_srgb,bool is_mipmaps,bool alpha_weighted_mips){
        if(!width || !height || rgba_pixels.size() != size_t(width) * height * 4) throw std::runtime_error("Invalid texture pixels");

        std::vector<bc_mip> mip_levels;

        while(true){
            bc_mip mip_level{width,height,{}};
            mip_level.blocks.reserve(size_t((width + 3) / 4) * ((height + 3) / 4) * (is_alpha ? 16 : 8));

            // 将当前mipmap按4x4 block压缩。
            for(uint32_t y = 0; y < height; y += 4) for(uint32_t x = 0; x < width; x += 4){
                std::array<std::array<uint8_t,4>,16> block_pixels{};

                // 边缘不足4x4时重复最外侧像素。
                for(uint32_t dy = 0; dy < 4; ++dy) for(uint32_t dx = 0; dx < 4; ++dx){
                    const size_t source = (size_t(std::min(y + dy,height - 1)) * width + std::min(x + dx,width - 1)) * 4;
                    std::copy_n(rgba_pixels.data() + source,4,block_pixels[dy * 4 + dx].data());
                }

                block(block_pixels,is_alpha,mip_level.blocks);
            }

            mip_levels.push_back(std::move(mip_level));

            if(!is_mipmaps || (width == 1 && height == 1)) break;

            // 生成下一级mipmap。
            const uint32_t next_width = std::max(1u,width / 2);
            const uint32_t next_height = std::max(1u,height / 2);
            std::vector<uint8_t> next_rgba(size_t(next_width) * next_height * 4);

            for(uint32_t y = 0; y < next_height; ++y) for(uint32_t x = 0; x < next_width; ++x){
                float sum[4]{};
                uint32_t sample_count = 0;

                for(uint32_t sy = y * height / next_height; sy < (y + 1) * height / next_height; ++sy)
                for(uint32_t sx = x * width / next_width; sx < (x + 1) * width / next_width; ++sx){
                    const auto* source_pixel = &rgba_pixels[(size_t(sy) * width + sx) * 4];
                    const float alpha = is_alpha ? source_pixel[3] / 255.0f : 1.0f;

                    for(int channel = 0; channel < 3; ++channel){
                        const float value = source_pixel[channel] / 255.0f;
                        sum[channel] += (is_srgb ? srgb_to_linear(value) : value) * (alpha_weighted_mips ? alpha : 1.0f);
                    }

                    sum[3] += alpha;
                    ++sample_count;
                }

                auto* target_pixel = &next_rgba[(size_t(y) * next_width + x) * 4];

                for(int channel = 0; channel < 3; ++channel){
                    const float weight=alpha_weighted_mips ? sum[3] : float(sample_count);
                    float value = weight > 0.0f ? sum[channel] / weight : 0.0f;
                    if(is_srgb) value = linear_to_srgb(value);
                    target_pixel[channel] = static_cast<uint8_t>(std::clamp(std::lround(value * 255.0f),0l,255l));
                }

                target_pixel[3] = is_alpha ? static_cast<uint8_t>(std::lround(sum[3] / sample_count * 255.0f)) : 255;
            }

            rgba_pixels = std::move(next_rgba);
            width = next_width;
            height = next_height;
        }

        return mip_levels;
    }
    /// @brief 将BC1/BC3压缩后的mipmap链封装为DDS文件二进制数据。
    /// @param mip_levels BC压缩后的mipmap链。
    /// @param is_alpha 是否使用alpha通道；false对应BC1，true对应BC3。
    /// @param is_srgb 是否使用sRGB格式。
    /// @return 完整DDS文件的二进制数据。
    std::vector<uint8_t> make_dds_bytes(const std::vector<bc_mip>& mip_levels,bool is_alpha,bool is_srgb){
        std::array<uint32_t,37> dds_header{};

        // DDS magic + DDS_HEADER大小。
        dds_header[0] = 0x20534444;
        dds_header[1] = 124;

        // DDS_HEADER flags。
        dds_header[2] = 0x81007 | (mip_levels.size() > 1 ? 0x20000 : 0);

        // 基础纹理尺寸。
        dds_header[3] = mip_levels.front().height;
        dds_header[4] = mip_levels.front().width;

        // 顶层mipmap的压缩数据大小和mipmap数量。
        dds_header[5] = static_cast<uint32_t>(mip_levels.front().blocks.size());
        dds_header[7] = static_cast<uint32_t>(mip_levels.size());

        // DDS_PIXELFORMAT：使用DX10扩展头。
        dds_header[19] = 32;
        dds_header[20] = 4;
        dds_header[21] = 0x30315844;

        // DDSCAPS_TEXTURE；存在mipmap时同时设置COMPLEX和MIPMAP。
        dds_header[27] = 0x1000 | (mip_levels.size() > 1 ? 0x400008 : 0);

        // DX10 header中的DXGI格式：
        // BC1_UNORM=71，BC1_UNORM_SRGB=72，BC3_UNORM=77，BC3_UNORM_SRGB=78。
        dds_header[32] = is_alpha ? (is_srgb ? 78 : 77) : (is_srgb ? 72 : 71);

        // TEXTURE2D。
        dds_header[33] = 3;

        // 单纹理，不是texture array。
        dds_header[35] = 1;

        // alpha模式：BC3使用straight alpha，BC1视为opaque。
        dds_header[36] = is_alpha ? 1 : 3;

        // 写入DDS头以及所有mipmap的BC block。
        std::vector<uint8_t> dds_bytes;
        for(uint32_t value : dds_header) append(dds_bytes,value,4);
        for(const auto& mip_level : mip_levels) dds_bytes.insert(dds_bytes.end(),mip_level.blocks.begin(),mip_level.blocks.end());

        return dds_bytes;
    }
}
