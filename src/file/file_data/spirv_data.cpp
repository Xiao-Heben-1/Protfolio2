#include <file/file_data/spirv_data.hpp>
#include <file/file_scan.hpp>
#include <file/tools/library_cache.hpp>
#include <engine.hpp>
#include <fstream>

namespace render{
    void spirv_data::register_library(const std::string& uid){
        const auto group = library_cache::hash(scan->get_library_path(uid).parent_path().generic_string());
        if(const auto old = entries.find(uid); old != entries.end()) pipelines[old->second.group].dirty = true;
        entries[uid] = spirv_entry{group};
        pipelines[group].dirty = true;
    }
    void spirv_data::remove_data(const std::string& uid){
        const auto it = entries.find(uid);
        if(it == entries.end()) return;
        pipelines[it->second.group].dirty = true;
        entries.erase(it);
    }
    void spirv_data::cleanup() noexcept{
        for(const auto& [group,entry] : pipelines) if(entry.pipeline != VK_NULL_HANDLE) vkDestroyPipeline(device,entry.pipeline,nullptr);
        pipelines.clear();
        entries.clear();
    }
    VkPipeline spirv_data::get_pipeline(const std::string& uid){
        const auto it = entries.find(uid);
        if(it == entries.end()) return VK_NULL_HANDLE;
        auto& entry = pipelines[it->second.group];
        if(entry.dirty){
            if(device == VK_NULL_HANDLE) device = egn.get_render_system().device;
            const auto replacement = create_pipeline(scan->get_library_path(uid).parent_path());
            if(entry.pipeline != VK_NULL_HANDLE) vkDestroyPipeline(device,entry.pipeline,nullptr);
            entry.pipeline = replacement;
            entry.dirty = false;
        }
        return entry.pipeline;
    }
    std::vector<uint32_t> spirv_data::read_spv(const std::filesystem::path& path){
        std::ifstream file(path, std::ios::ate | std::ios::binary);
        const size_t size = static_cast<size_t>(file.tellg());
        std::vector<uint32_t> code(size / sizeof(uint32_t));
        file.seekg(0);
        file.read(reinterpret_cast<char*>(code.data()), code.size() * sizeof(uint32_t));
        return code;
    }

    VkShaderStageFlagBits spirv_data::parse_stage(const std::filesystem::path& path){
        const std::string name = path.filename().string();
        if(name.ends_with(".vert.spv")) return VK_SHADER_STAGE_VERTEX_BIT;
        if(name.ends_with(".frag.spv")) return VK_SHADER_STAGE_FRAGMENT_BIT;
        if(name.ends_with(".geom.spv")) return VK_SHADER_STAGE_GEOMETRY_BIT;
        if(name.ends_with(".tesc.spv")) return VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT;
        if(name.ends_with(".tese.spv")) return VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT;
        return VK_SHADER_STAGE_FLAG_BITS_MAX_ENUM;
    }

    VkPipeline spirv_data::create_pipeline(const std::filesystem::path& path){
        struct module_scope{
            VkDevice device;
            std::vector<VkShaderModule> modules;
            ~module_scope(){
                for(VkShaderModule module : modules) vkDestroyShaderModule(device,module,nullptr);
            }
        } loaded{device,{}};
        std::vector<VkPipelineShaderStageCreateInfo> shader_stages;
        for(const auto& entry : std::filesystem::directory_iterator(path)){
            if(!entry.is_regular_file()) continue;
            const VkShaderStageFlagBits stage = parse_stage(entry.path());
            if(stage == VK_SHADER_STAGE_FLAG_BITS_MAX_ENUM) continue;

            const auto code = read_spv(entry.path());
            VkShaderModuleCreateInfo module_info{};
            module_info.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
            module_info.codeSize = code.size() * sizeof(uint32_t);
            module_info.pCode = code.data();

            VkShaderModule module = VK_NULL_HANDLE;
            vkCreateShaderModule(device, &module_info, nullptr, &module);
            loaded.modules.push_back(module);

            VkPipelineShaderStageCreateInfo stage_info{};
            stage_info.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
            stage_info.stage = stage;
            stage_info.module = module;
            stage_info.pName = "main";
            shader_stages.push_back(stage_info);
        }

        // 管线状态与布局由 render_system 管理，附件格式来自它的 camera_data。
        return egn.get_render_system().create_pipeline(shader_stages);
    }
}
