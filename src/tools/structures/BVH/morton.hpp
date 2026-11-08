#ifndef __RENDER_MORTON_HPP
#define __RENDER_MORTON_HPP
#include <cstdint>
#include <cmath>
#include <glm/glm.hpp>
#include <tools/structures/AABB.hpp>
namespace render{

struct morton final{
    static constexpr double cell_size = 128.0;
    static constexpr int32_t grid_min32 = -512, grid_max32 = 511;
    static constexpr int32_t grid_min64 = -1048576, grid_max64 = 1048575;
    // 半开世界范围：32 位 [-65536,65536)，64 位 [-134217728,134217728)。
    // 无运行时检查；调用者保证坐标有效，负坐标使用 floor。
    static int32_t grid_coordinate(double value){
        return static_cast<int32_t>(std::floor(value / cell_size));
    }
    static uint64_t grid_to_morton64(int32_t x,int32_t y,int32_t z){
        return morton_encode64(uint64_t(int64_t(x)-grid_min64),
            uint64_t(int64_t(y)-grid_min64),uint64_t(int64_t(z)-grid_min64));
    }
    //Morton编码处理，64位版按3倍间隔插入0，最多21位输入
    inline static uint64_t left_shift64(uint64_t x){
        x = (x | (x << 32)) & 0x1F00000000FFFFULL;
        x = (x | (x << 16)) & 0x1F0000FF0000FFULL;
        x = (x | (x << 8))  & 0x100F00F00F00F00FULL;
        x = (x | (x << 4))  & 0x10C30C30C30C30C3ULL;
        x = (x | (x << 2))  & 0x1249249249249249ULL;
        return x;
    }

    //Morton编码处理，32位版按3倍间隔插入0，最多10位输入
    inline static uint32_t left_shift32(uint32_t x){
        x = (x | (x << 16)) & 0x030000FF;
        x = (x | (x << 8))  & 0x0300F00F;
        x = (x | (x << 4))  & 0x030C30C3;
        x = (x | (x << 2))  & 0x09249249;
        return x;
    }

    //生成32位Morton编码
    inline static uint32_t morton_encode32(uint32_t x, uint32_t y, uint32_t z){
        return (left_shift32(z) << 2) | (left_shift32(y) << 1) | left_shift32(x);
    }

    //生成64位Morton编码
    inline static uint64_t morton_encode64(uint64_t x, uint64_t y, uint64_t z){
        return (left_shift64(z) << 2) | (left_shift64(y) << 1) | left_shift64(x);
    }

    //将坐标按128m网格转为32位Morton编码
    inline static uint32_t position_to_morton32(const glm::vec3& v){
        constexpr int32_t bias = 1 << 9;//10bit：[-512, 511]
        int32_t grid_x = grid_coordinate(v.x);
        int32_t grid_y = grid_coordinate(v.y);
        int32_t grid_z = grid_coordinate(v.z);
        uint32_t x = static_cast<uint32_t>(grid_x + bias);
        uint32_t y = static_cast<uint32_t>(grid_y + bias);
        uint32_t z = static_cast<uint32_t>(grid_z + bias);
        return morton_encode32(x, y, z);
    }
    //将坐标按128m网格转为64位Morton编码
    inline static uint64_t position_to_morton64(const glm::vec3& v){
        constexpr int64_t bias = 1LL << 20;//21bit：[-1048576, 1048575]
        int64_t grid_x = grid_coordinate(v.x);
        int64_t grid_y = grid_coordinate(v.y);
        int64_t grid_z = grid_coordinate(v.z);
        uint64_t x = static_cast<uint64_t>(grid_x + bias);
        uint64_t y = static_cast<uint64_t>(grid_y + bias);
        uint64_t z = static_cast<uint64_t>(grid_z + bias);
        return morton_encode64(x, y, z);
    }
};

}

#endif
