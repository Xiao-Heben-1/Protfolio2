#ifndef __RENDER_SHADER_HPP
#define __RENDER_SHADER_HPP

#include <vulkan/vulkan.h>
#include <string>

namespace render{
struct shader final{
    /// @brief 顶点阶段对应资源UID
    std::string vert_uid;

    /// @brief 片元阶段对应资源UID
    std::string frag_uid;

    /// @brief 几何阶段对应资源UID
    std::string geom_uid;

    /// @brief 细分控制阶段UID
    std::string tess_control_uid;

    /// @brief 细分求值阶段UID
    std::string tess_evaluation_uid;

    /// @brief 计算阶段UID
    std::string comp_uid;

    /// @brief 已解析的运行时资源地址
    VkPipeline* vert = nullptr;
    VkPipeline* frag = nullptr;
    VkPipeline* geom = nullptr;
    VkPipeline* tess_control = nullptr;
    VkPipeline* tess_evaluation = nullptr;
    VkPipeline* comp = nullptr;
};
}

#endif
