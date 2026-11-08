#ifndef __RENDER_SPIRV_DATA_HPP
#define __RENDER_SPIRV_DATA_HPP
#include <file/file_data/file_data.hpp>
#include <cstdint>
#include <unordered_map>
#include <vector>

namespace render{
    class file_resources;
    // library 中的编译文件按 UID 注册，同目录的阶段共享一条图形管线。
    // 阶段文件后缀：.vert.spv、.frag.spv、.geom.spv、.tesc.spv、.tese.spv。
    class spirv_data final:public file_data{
        friend class file_resources;
    public:
        struct spirv_entry{
            std::string group;//阶段目录的分组标识，实际路径按 UID 向 scan 查询
        };
        const auto& get_entries() const noexcept{return entries;}
        // 首次使用或文件变更后创建；调用前 GPU 须停止使用旧管线。
        VkPipeline get_pipeline(const std::string& uid);
    private:
        struct pipeline_entry{
            VkPipeline pipeline = VK_NULL_HANDLE;
            bool dirty = true;
        };
        // key 是文件 UID，value 是所属管线分组；map 持有 entry 本身。
        // 通过 entries.find(uid) 查找，entry 内不再重复保存 UID。
        std::unordered_map<std::string,spirv_entry> entries;
        // key 是目录分组的哈希，同一目录的多个阶段共用一条管线。
        std::unordered_map<std::string,pipeline_entry> pipelines;
        void register_library(const std::string& uid) override;
        
        explicit spirv_data(VkDevice device):file_data(device){}
        ~spirv_data(){cleanup();}

        spirv_data(const spirv_data&) = delete;
        spirv_data& operator=(const spirv_data&) = delete;
        spirv_data(spirv_data&&) = delete;
        spirv_data& operator=(spirv_data&&) = delete;

        // 编译与持久化由 scan 完成，data 只接收已生成的 library 资源。
        void register_data(const std::string&) override{}
        void remove_data(const std::string& uid) override;
        void cleanup() noexcept override;

        VkPipeline create_pipeline(const std::filesystem::path& directory);
        static std::vector<uint32_t> read_spv(const std::filesystem::path& path);
        static VkShaderStageFlagBits parse_stage(const std::filesystem::path& path);
    };
}

#endif // __RENDER_SPIRV_DATA_HPP
