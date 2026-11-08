#pragma once
#include <engine.hpp>
#include <imgui.h>
#include <imgui_impl_vulkan.h>
#include <memory>
#include <string>
#include <vector>
struct GLFWwindow;
namespace render::front {
struct scene_item {
    object_config config;
    std::unique_ptr<object> runtime;
};
class editor {
    GLFWwindow* window=nullptr;
    render_context gpu;
    ImGui_ImplVulkanH_Window swapchain;
    VkDescriptorPool descriptor_pool=VK_NULL_HANDLE;
    VkDescriptorSet scene_texture=VK_NULL_HANDLE;
    VkExtent2D requested_extent{960,540};
    bool rebuild_swapchain=false,show_global_passes=false,show_grid=true,scene_dirty=false;
    bool imgui_ready=false,renderer_ready=false;
    int selected_object=-1;
    bool camera_selected=true;
    bool lighting_selected=false;
    std::string selected_asset;
    scene_config scene_document;
    std::filesystem::path scene_path="generated/scenes/default.scene";
    bool library_view=false;
    bool component_smoke=false,component_smoke_complete=false;
    int component_smoke_index=0;
    ImVec2 add_component_button{},component_menu_entry{};
    void component_menu(scene_item& item);
    void add_component(scene_item& item,component_type type);
    void component_smoke_input();
    std::vector<scene_item> objects;
    std::vector<std::string> messages;
    pass_config pass_draft;
    std::string selected_pass;
    char project_search[128]{};
    std::string ini_path;
    uint64_t frame_count=0;
    ImGuiID dock_id=0;
public:
    editor()=default;
    ~editor();
    int run(workspace_config config,int frame_limit=0,bool smoke=false,const std::filesystem::path& model={});
private:
    void initialize(workspace_config config,bool hidden);
    void initialize_ui();
    void component_inspector(scene_item& item);
    void lighting_inspector();
    void camera_inspector(camera& value);
    void light_inspector(light& value);
    void choose_model();
    void add_geometry(const char* name);
    void load_model(const std::filesystem::path& path);
    void create_object_menu();
    void add_object_asset(const std::string& guid);
    void resize_swapchain(int width,int height);
    void render_ui();
    void shutdown();
    void draw();
    void menu();
    void hierarchy();
    void viewport();
    void inspector();
    void project();
    void console();
    void global_passes();
    void default_layout(ImGuiID dock);
    void open_scene(const std::filesystem::path& path);
    void save_scene(bool authored=true);
    void new_scene();
    void add_model(const std::string& guid);
    void add_object(object_config config);
    void import_path(const std::filesystem::path& path);
    void focus_selected();
    void log(std::string message);
    void edit_asset();
    void select_pass(const std::string& guid);
    static void drop_files(GLFWwindow* window,int count,const char** paths);
};
}
