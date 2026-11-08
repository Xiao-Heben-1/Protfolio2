#include <front/editor.hpp>
#include <imgui_impl_glfw.h>
#include <imgui_internal.h>
#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <file/tools/file_path.hpp>
namespace render::front {
namespace {void check(VkResult status){if(status<0)throw std::runtime_error("Vulkan error "+std::to_string(status));}}
editor::~editor(){shutdown();}
void editor::log(std::string message){messages.push_back(std::move(message));if(messages.size()>200)messages.erase(messages.begin());}
void editor::initialize(workspace_config config,bool hidden){
    if(!glfwInit())throw std::runtime_error("GLFW initialization failed");
    glfwWindowHint(GLFW_CLIENT_API,GLFW_NO_API);
    glfwWindowHint(GLFW_VISIBLE,hidden ? GLFW_FALSE : GLFW_TRUE);
    window=glfwCreateWindow(1440,900,"Render Editor",nullptr,nullptr);
    if(!window)throw std::runtime_error("Window creation failed");
    glfwSetWindowUserPointer(window,this);glfwSetDropCallback(window,drop_files);
    egn.init(std::move(config));
    auto& renderer=egn.get_render_system();
    uint32_t count=0;const auto* extensions=glfwGetRequiredInstanceExtensions(&count);
    render_window_info info;info.instance_extensions.assign(extensions,extensions+count);
    info.create_surface=[&](VkInstance instance){VkSurfaceKHR surface;check(glfwCreateWindowSurface(instance,window,nullptr,&surface));return surface;};
    renderer.init(egn.get_file_system().get_config().builtin_shader_root,info);renderer_ready=true;gpu=renderer.get_context();
    renderer.resize_viewport(requested_extent);
    swapchain.Surface=gpu.surface;
    const VkFormat formats[]={VK_FORMAT_B8G8R8A8_UNORM,VK_FORMAT_R8G8B8A8_UNORM};
    swapchain.SurfaceFormat=ImGui_ImplVulkanH_SelectSurfaceFormat(gpu.physical_device,gpu.surface,formats,2,VK_COLOR_SPACE_SRGB_NONLINEAR_KHR);
    const VkPresentModeKHR present[]={VK_PRESENT_MODE_FIFO_KHR};
    swapchain.PresentMode=ImGui_ImplVulkanH_SelectPresentMode(gpu.physical_device,gpu.surface,present,1);
    int width,height;glfwGetFramebufferSize(window,&width,&height);resize_swapchain(width,height);
    initialize_ui();
    const auto& workspace=egn.get_file_system().get_config();
    if(std::filesystem::exists(workspace.library_root/workspace.startup_scene))open_scene(workspace.startup_scene);
    else {new_scene();scene_path=workspace.library_root/workspace.startup_scene;save_scene(false);}
    log("Source: "+path_to_utf8(workspace.source_root));
    log("Library: "+path_to_utf8(workspace.library_root));
    log("RMB + WASD/QE: fly | MMB: pan | Wheel: dolly | F: frame selection");
}
void editor::initialize_ui(){
    VkDescriptorPoolSize sizes[]={{VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,128}};
    VkDescriptorPoolCreateInfo pool{VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO};pool.flags=VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
    pool.maxSets=128;pool.poolSizeCount=1;pool.pPoolSizes=sizes;check(vkCreateDescriptorPool(gpu.device,&pool,nullptr,&descriptor_pool));
    IMGUI_CHECKVERSION();ImGui::CreateContext();
    auto& io=ImGui::GetIO();io.ConfigFlags|=ImGuiConfigFlags_DockingEnable|ImGuiConfigFlags_NavEnableKeyboard;
    ini_path=(egn.get_file_system().get_config().library_root/"editor-layout-v2.ini").string();io.IniFilename=ini_path.c_str();
    if(std::filesystem::exists("C:/Windows/Fonts/msyh.ttc"))io.Fonts->AddFontFromFileTTF("C:/Windows/Fonts/msyh.ttc",22,nullptr,io.Fonts->GetGlyphRangesChineseFull());
    else io.Fonts->AddFontFromFileTTF("C:/Windows/Fonts/segoeui.ttf",22);
    ImGui::StyleColorsDark();
    auto& style=ImGui::GetStyle();style.WindowRounding=3;style.FrameRounding=3;style.GrabRounding=3;style.FramePadding={7,5};
    style.WindowPadding={10,10};style.ItemSpacing={8,7};style.WindowBorderSize=0;style.TabRounding=3;
    auto* colors=style.Colors;
    colors[ImGuiCol_WindowBg]={.22f,.22f,.22f,1};colors[ImGuiCol_ChildBg]={.18f,.18f,.18f,1};
    colors[ImGuiCol_Header]={.28f,.28f,.28f,1};colors[ImGuiCol_HeaderHovered]={.34f,.34f,.34f,1};
    colors[ImGuiCol_HeaderActive]={.21f,.43f,.64f,1};colors[ImGuiCol_Button]={.32f,.32f,.32f,1};
    colors[ImGuiCol_ButtonHovered]={.24f,.39f,.55f,1};colors[ImGuiCol_CheckMark]={.38f,.72f,1,1};
    colors[ImGuiCol_TabActive]={.28f,.28f,.28f,1};colors[ImGuiCol_Tab]={.18f,.18f,.18f,1};
    colors[ImGuiCol_FrameBg]={.15f,.15f,.15f,1};colors[ImGuiCol_TitleBgActive]={.18f,.18f,.18f,1};colors[ImGuiCol_MenuBarBg]={.17f,.17f,.17f,1};
    ImGui_ImplGlfw_InitForVulkan(window,true);
    ImGui_ImplVulkan_InitInfo init{};init.Instance=gpu.instance;init.PhysicalDevice=gpu.physical_device;init.Device=gpu.device;
    init.QueueFamily=gpu.queue_family;init.Queue=gpu.queue;init.DescriptorPool=descriptor_pool;init.MinImageCount=2;init.ImageCount=swapchain.ImageCount;
    init.MSAASamples=VK_SAMPLE_COUNT_1_BIT;init.CheckVkResultFn=check;ImGui_ImplVulkan_Init(&init,swapchain.RenderPass);imgui_ready=true;
    auto& frame=swapchain.Frames[0];
    VkCommandBufferBeginInfo begin{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};begin.flags=VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    check(vkBeginCommandBuffer(frame.CommandBuffer,&begin));ImGui_ImplVulkan_CreateFontsTexture(frame.CommandBuffer);check(vkEndCommandBuffer(frame.CommandBuffer));
    VkSubmitInfo submit{VK_STRUCTURE_TYPE_SUBMIT_INFO};submit.commandBufferCount=1;submit.pCommandBuffers=&frame.CommandBuffer;
    check(vkQueueSubmit(gpu.queue,1,&submit,VK_NULL_HANDLE));check(vkQueueWaitIdle(gpu.queue));ImGui_ImplVulkan_DestroyFontUploadObjects();
    const auto image=egn.get_render_system().get_color_image();
    scene_texture=ImGui_ImplVulkan_AddTexture(image.sampler,image.view,VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
}
void editor::resize_swapchain(int width,int height){
    ImGui_ImplVulkanH_CreateOrResizeWindow(gpu.instance,gpu.physical_device,gpu.device,&swapchain,gpu.queue_family,nullptr,width,height,2);
    swapchain.FrameIndex=0;swapchain.SemaphoreIndex=0;rebuild_swapchain=false;
}
void editor::render_ui(){
    auto& semaphores=swapchain.FrameSemaphores[swapchain.SemaphoreIndex];
    auto status=vkAcquireNextImageKHR(gpu.device,swapchain.Swapchain,UINT64_MAX,semaphores.ImageAcquiredSemaphore,VK_NULL_HANDLE,&swapchain.FrameIndex);
    if(status==VK_ERROR_OUT_OF_DATE_KHR){rebuild_swapchain=true;return;}check(status);
    auto& frame=swapchain.Frames[swapchain.FrameIndex];
    check(vkWaitForFences(gpu.device,1,&frame.Fence,VK_TRUE,UINT64_MAX));check(vkResetFences(gpu.device,1,&frame.Fence));
    check(vkResetCommandPool(gpu.device,frame.CommandPool,0));
    VkCommandBufferBeginInfo begin{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};begin.flags=VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    check(vkBeginCommandBuffer(frame.CommandBuffer,&begin));
    VkRenderPassBeginInfo pass{VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO};pass.renderPass=swapchain.RenderPass;pass.framebuffer=frame.Framebuffer;
    pass.renderArea.extent={uint32_t(swapchain.Width),uint32_t(swapchain.Height)};
    VkClearValue clear{{{.08f,.09f,.11f,1}}};pass.clearValueCount=1;pass.pClearValues=&clear;
    vkCmdBeginRenderPass(frame.CommandBuffer,&pass,VK_SUBPASS_CONTENTS_INLINE);
    ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(),frame.CommandBuffer);vkCmdEndRenderPass(frame.CommandBuffer);
    check(vkEndCommandBuffer(frame.CommandBuffer));
    const VkPipelineStageFlags wait=VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    VkSubmitInfo submit{VK_STRUCTURE_TYPE_SUBMIT_INFO};submit.waitSemaphoreCount=1;submit.pWaitSemaphores=&semaphores.ImageAcquiredSemaphore;
    submit.pWaitDstStageMask=&wait;submit.commandBufferCount=1;submit.pCommandBuffers=&frame.CommandBuffer;
    submit.signalSemaphoreCount=1;submit.pSignalSemaphores=&semaphores.RenderCompleteSemaphore;check(vkQueueSubmit(gpu.queue,1,&submit,frame.Fence));
    VkPresentInfoKHR present{VK_STRUCTURE_TYPE_PRESENT_INFO_KHR};present.waitSemaphoreCount=1;present.pWaitSemaphores=&semaphores.RenderCompleteSemaphore;
    present.swapchainCount=1;present.pSwapchains=&swapchain.Swapchain;present.pImageIndices=&swapchain.FrameIndex;
    status=vkQueuePresentKHR(gpu.queue,&present);
    if(status==VK_ERROR_OUT_OF_DATE_KHR || status==VK_SUBOPTIMAL_KHR)rebuild_swapchain=true;else check(status);
    check(vkQueueWaitIdle(gpu.queue));
    swapchain.SemaphoreIndex=(swapchain.SemaphoreIndex+1)%swapchain.ImageCount;
}
int editor::run(workspace_config config,int frame_limit,bool smoke,const std::filesystem::path& model){
    if(smoke && frame_limit<100)frame_limit=100;
    component_smoke=smoke;
    initialize(std::move(config),smoke);
    if(!model.empty())load_model(model);
    const size_t smoke_subject=model.empty() ? 0 : objects.size()-1;
    bool smoke_camera_moved=false;
    bool smoke_transparent=false;
    std::vector<material_uniform_data> smoke_materials;
    while(!glfwWindowShouldClose(window) && (!frame_limit || frame_count<uint64_t(frame_limit))){
        glfwPollEvents();
        int width,height;glfwGetFramebufferSize(window,&width,&height);
        if(width==0 || height==0){glfwWaitEventsTimeout(.05);continue;}
        if(rebuild_swapchain || width!=swapchain.Width || height!=swapchain.Height)resize_swapchain(width,height);
        auto& renderer=egn.get_render_system();
        const auto image=renderer.get_color_image();
        if(requested_extent.width!=image.extent.width || requested_extent.height!=image.extent.height){
            ImGui_ImplVulkan_RemoveTexture(scene_texture);renderer.resize_viewport(requested_extent);
            const auto replacement=renderer.get_color_image();scene_texture=ImGui_ImplVulkan_AddTexture(replacement.sampler,replacement.view,VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
        }
        try{egn.get_file_system().update();}catch(const std::exception& e){log(e.what());}
        ImGui_ImplVulkan_NewFrame();ImGui_ImplGlfw_NewFrame();
        if(smoke)component_smoke_input();
        ImGui::NewFrame();
        draw();
        if(smoke && frame_count==9 && !objects.empty()){
            auto& obj=*objects[smoke_subject].runtime;smoke_transparent=obj.get_transparent();obj.set_transparent(true);
            if(auto* mat=obj.get_material())for(auto* sub:mat->get_sub_materials()){
                smoke_materials.push_back(sub->uniform_data);sub->uniform_data.base_color.a=.4f;sub->uniform_data.texture_flags_2.z=3;
            }
        }
        if(smoke && frame_count==15 && !objects.empty()){
            auto& obj=*objects[smoke_subject].runtime;obj.set_transparent(smoke_transparent);
            if(auto* mat=obj.get_material())for(size_t i=0;i<smoke_materials.size();++i)mat->get_sub_material(uint32_t(i))->uniform_data=smoke_materials[i];
        }
        const auto before=renderer.get_scene().get_main_camera().get_position();
        if(smoke && frame_count==2){auto& input=renderer.get_scene().get_main_camera().get_behavior().get_input();input.move={1,0,0};input.delta_time=.1f;input.zoom=1;}
        if(smoke && frame_count==3 && !objects.empty()){selected_object=int(smoke_subject);camera_selected=false;focus_selected();}
        renderer.pass();
        if(smoke && frame_count==2)smoke_camera_moved=glm::length(renderer.get_scene().get_main_camera().get_position()-before)>.1f;
        ImGui::Render();render_ui();++frame_count;
        if(smoke && frame_count==5)glfwSetWindowSize(window,1360,860);
        if(smoke && frame_count==82){
            for(const auto& [guid,file]:egn.get_file_system().get_files())if(file.type==file_type::render_pass){
                auto* pass=egn.get_file_system().get_resources().get_render_pass_data().get_pass(guid);
                if(pass->builtin=="transparent_resolve"){select_pass(guid);break;}
            }
        }
    }
    if(smoke){
        if(!component_smoke_complete)throw std::runtime_error("Add Component UI test did not complete");
        if(!smoke_camera_moved)throw std::runtime_error("Camera input was not applied");
        for(const auto& item:objects)if(const auto* mesh=item.runtime->get_mesh();mesh && !mesh->get_vertex_buffer())
            throw std::runtime_error("Scene mesh did not become GPU resident");
        save_scene(false);
        const auto restored=scene_config::load(egn.get_file_system().get_config().library_root/scene_path);
        if(restored.objects.size()!=objects.size() || restored.main_camera.position!=scene_document.main_camera.position)
            throw std::runtime_error("Scene round-trip failed");
        std::cout<<"Editor smoke: "<<frame_count<<" frames, "<<objects.size()<<" objects, Add Component UI + seven component persistence, camera navigation, GPU meshes, resize and scene round-trip passed\n";
    }
    return 0;
}
void editor::shutdown(){
    if(gpu.device)vkDeviceWaitIdle(gpu.device);
    objects.clear();
    if(imgui_ready){ImGui_ImplVulkan_RemoveTexture(scene_texture);ImGui_ImplVulkan_Shutdown();ImGui_ImplGlfw_Shutdown();ImGui::DestroyContext();imgui_ready=false;}
    if(descriptor_pool)vkDestroyDescriptorPool(gpu.device,descriptor_pool,nullptr);
    if(swapchain.Surface)ImGui_ImplVulkanH_DestroyWindow(gpu.instance,gpu.device,&swapchain,nullptr);
    if(renderer_ready)egn.shutdown();
    if(window){glfwDestroyWindow(window);window=nullptr;glfwTerminate();}
    descriptor_pool=VK_NULL_HANDLE;gpu={};renderer_ready=false;
}
void editor::drop_files(GLFWwindow* handle,int count,const char** paths){
    auto* self=static_cast<editor*>(glfwGetWindowUserPointer(handle));
    for(int i=0;i<count;++i)try{self->import_path(path_from_utf8(paths[i]));}catch(const std::exception& e){self->log(e.what());}
}
}
