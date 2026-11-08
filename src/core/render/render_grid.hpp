#ifndef __RENDER_RENDER_GRID_HPP
#define __RENDER_RENDER_GRID_HPP

#include <unordered_map>
#include <cstdint>
#include <vector>
namespace render{
    class object;
    class render_grid{
        friend class object;
    private:
        std::unordered_map<uint64_t, std::vector<uint32_t>> grids;
    };
}


#endif
