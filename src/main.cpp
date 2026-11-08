#include <engine.hpp>
#include <iostream>

int main(int argc,char** argv){
    if(argc != 2){
        std::cout << "Usage: Render <workspace-directory | workspace.json>\nImports resources and updates library caches.\n";
        return argc == 1 ? 0 : 1;
    }
    const auto path=std::filesystem::path(argv[1]);
    if(std::filesystem::is_directory(path))render::egn.init(path);
    else render::egn.init(render::workspace_config::load(path));
    render::egn.get_file_system().update();
    std::cout << "Library import complete.\n";
    return 0;
}
