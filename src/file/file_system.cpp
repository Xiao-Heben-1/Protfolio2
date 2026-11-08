#include <file/file_system.hpp>

namespace render{
    file_system::file_system(const std::filesystem::path& path,VkDevice device):
        file_system(workspace_config{path},device){}
    file_system::file_system(workspace_config value,VkDevice device):config(std::move(value)),res(device),fs(*this){
        config.resolve(std::filesystem::current_path());
        std::filesystem::create_directories(config.source_root);
        fs.init(config.source_root);
        fs.generate_system_config();
    }
    file_system::~file_system(){fs.stop_monitor();}
    void file_system::update(){fs.process_events();}
}
