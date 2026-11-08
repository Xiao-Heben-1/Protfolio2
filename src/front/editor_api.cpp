#include <front/editor_api.hpp>
#include <front/editor.hpp>
#include <iostream>
extern "C" int render_run_editor(const wchar_t* workspace,int frame_limit,int smoke,const wchar_t* model){
    try{
        const std::filesystem::path input=workspace;
        auto config=render::workspace_config::load(input.extension()==".json" ? input : input/"workspace.json");
        render::front::editor app;
        return app.run(std::move(config),frame_limit,smoke!=0,model ? std::filesystem::path(model) : std::filesystem::path());
    }catch(const std::exception& error){std::cerr<<"Render: "<<error.what()<<'\n';return 1;}
}
