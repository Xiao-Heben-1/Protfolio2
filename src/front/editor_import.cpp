#include <front/editor.hpp>
#include <core/object/object_loader.hpp>
#include <algorithm>
#include <file/tools/file_path.hpp>
#define GLFW_INCLUDE_NONE
#define GLFW_EXPOSE_NATIVE_WIN32
#include <GLFW/glfw3.h>
#include <GLFW/glfw3native.h>
#include <commdlg.h>
#include <array>
namespace render::front {
void editor::choose_model(){
    std::array<wchar_t,32768> filename{};
    const auto initial=egn.get_file_system().get_config().workspace_root;
    OPENFILENAMEW dialog{};dialog.lStructSize=sizeof(dialog);dialog.hwndOwner=glfwGetWin32Window(window);
    dialog.lpstrFilter=L"Models (*.fbx;*.obj)\0*.fbx;*.obj\0FBX\0*.fbx\0OBJ\0*.obj\0\0";
    dialog.lpstrFile=filename.data();dialog.nMaxFile=DWORD(filename.size());dialog.lpstrInitialDir=initial.c_str();
    dialog.lpstrTitle=L"load_model";dialog.Flags=OFN_FILEMUSTEXIST|OFN_PATHMUSTEXIST|OFN_NOCHANGEDIR;
    if(GetOpenFileNameW(&dialog))try{load_model(std::filesystem::path(filename.data()));}catch(const std::exception& e){log(e.what());}
}
void editor::add_geometry(const char* name){
    constexpr const char* names[]={"Cube","Sphere","Capsule","Cylinder","Plane","Quad"};
    const auto index=std::find(std::begin(names),std::end(names),std::string(name))-std::begin(names);
    std::unique_ptr<object> runtime(object_loader::create_geometry(geometry_type(index)));
    object_config config;config.name=name;config=object_loader::describe(*runtime,config);
    objects.push_back({std::move(config),std::move(runtime)});selected_object=int(objects.size())-1;
    camera_selected=false;lighting_selected=false;selected_asset.clear();scene_dirty=true;
}
}
