#include <front/editor_api.hpp>
#include <windows.h>
#include <shellapi.h>
#include <filesystem>
#include <string>
int main(){
    std::wstring executable(32768,L'\0');executable.resize(GetModuleFileNameW(nullptr,executable.data(),DWORD(executable.size())));
    std::filesystem::path workspace=std::filesystem::path(executable).parent_path()/"workspace.json";
    std::wstring model;int frames=0,smoke=0,count=0;auto** args=CommandLineToArgvW(GetCommandLineW(),&count);
    for(int i=1;i<count;++i){const std::wstring arg=args[i];
        if(arg==L"--frames" && i+1<count)frames=std::stoi(args[++i]);
        else if(arg==L"--smoke")smoke=1;
        else if(arg==L"--load-model" && i+1<count)model=args[++i];
        else workspace=arg;
    }
    LocalFree(args);return render_run_editor(workspace.c_str(),frames,smoke,model.empty() ? nullptr : model.c_str());
}
