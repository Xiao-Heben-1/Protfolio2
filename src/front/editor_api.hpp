#pragma once
#ifdef RENDER_RUNTIME_EXPORTS
#define RENDER_RUNTIME_API __declspec(dllexport)
#else
#define RENDER_RUNTIME_API __declspec(dllimport)
#endif
// DLL 内持有整个运行时；调用方只传配置路径，不跨 DLL 传递 STL/ImGui/Vulkan 对象。
extern "C" RENDER_RUNTIME_API int render_run_editor(const wchar_t* workspace,int frame_limit=0,int smoke=0,const wchar_t* model=nullptr);
