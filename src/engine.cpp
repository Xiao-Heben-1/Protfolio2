#include <engine.hpp>
namespace render{
    engine egn;
    void engine::shutdown(){
        fs.reset();
        rd.shutdown();
    }
    void engine::init(workspace_config config){
        fs = std::make_unique<file_system>(std::move(config));
    }
    void engine::init(const std::filesystem::path& project){
        if(std::filesystem::exists(project/"workspace.json"))init(workspace_config::load(project/"workspace.json"));
        else fs = std::make_unique<file_system>(project);
    }
}
