#ifndef __RENDER_FILE_CONFIG_HPP
#define __RENDER_FILE_CONFIG_HPP

#include <cstdint>
#include <string>
#include <file/tools/guid.hpp>

namespace render{
    // 持久化配置的公共信息。guid不使用运行时run_id，version只表示配置格式。
    struct file_config{
        uint32_t version = 1;
        std::string guid=generate_guid();
        std::string name;
    };
}

#endif // __RENDER_FILE_CONFIG_HPP
