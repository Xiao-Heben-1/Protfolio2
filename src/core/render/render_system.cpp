#include <core/render/render_system.hpp>
#include <core/component/component_enter.hpp>
#include <core/object/object_enter.hpp>
#include <cstddef>
#include <stdexcept>
#include <string>
#include <filesystem>
#include <fstream>
#include <unordered_map>
#include <unordered_set>
#include <algorithm>
#include <engine.hpp>

namespace render{
void render_system::init_dev(const render_window_info& window){
    //创建 Vulkan instance
    VkApplicationInfo app_info{};
    app_info.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    app_info.pApplicationName = "Render";
    app_info.applicationVersion = VK_MAKE_VERSION(1,0,0);
    app_info.pEngineName = "Render";
    app_info.engineVersion = VK_MAKE_VERSION(1,0,0);
    app_info.apiVersion = VK_API_VERSION_1_3;

    VkInstanceCreateInfo instance_info{};
    instance_info.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    instance_info.pApplicationInfo = &app_info;
    instance_info.enabledExtensionCount=uint32_t(window.instance_extensions.size());
    instance_info.ppEnabledExtensionNames=window.instance_extensions.data();

    if(vkCreateInstance(&instance_info,nullptr,&instance) != VK_SUCCESS){
        throw std::runtime_error("Failed to create Vulkan instance");
    }

    if(window.create_surface)context.surface=window.create_surface(instance);

    //选择 physical device
    uint32_t physical_device_count = 0;
    vkEnumeratePhysicalDevices(instance,&physical_device_count,nullptr);

    if(physical_device_count == 0){
        throw std::runtime_error("No Vulkan physical device found");
    }

    std::vector<VkPhysicalDevice> physical_devices(physical_device_count);
    vkEnumeratePhysicalDevices(instance,&physical_device_count,physical_devices.data());

    std::stable_sort(physical_devices.begin(),physical_devices.end(),[](auto a,auto b){
        VkPhysicalDeviceProperties pa{},pb{};vkGetPhysicalDeviceProperties(a,&pa);vkGetPhysicalDeviceProperties(b,&pb);
        return (pa.deviceType==VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU)>(pb.deviceType==VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU);
    });
    uint32_t graphics_queue_family = UINT32_MAX;

    for(VkPhysicalDevice target : physical_devices){
        uint32_t queue_family_count = 0;
        vkGetPhysicalDeviceQueueFamilyProperties(target,&queue_family_count,nullptr);

        std::vector<VkQueueFamilyProperties> queue_families(queue_family_count);
        vkGetPhysicalDeviceQueueFamilyProperties(target,&queue_family_count,queue_families.data());

        for(uint32_t i = 0;i < queue_family_count;i++){
            VkBool32 present=VK_TRUE;
            if(context.surface)vkGetPhysicalDeviceSurfaceSupportKHR(target,i,context.surface,&present);
            if(!present)continue;
            if((queue_families[i].queueFlags & (VK_QUEUE_GRAPHICS_BIT | VK_QUEUE_COMPUTE_BIT)) == (VK_QUEUE_GRAPHICS_BIT | VK_QUEUE_COMPUTE_BIT)){
                physical_device = target;
                graphics_queue_family = i;
                break;
            }
        }

        if(physical_device != VK_NULL_HANDLE) break;
    }

    if(physical_device == VK_NULL_HANDLE){
        throw std::runtime_error("No Vulkan graphics physical device found");
    }

    //创建 logical device
    context.queue_family=graphics_queue_family;
    float queue_priority = 1.0f;

    VkDeviceQueueCreateInfo queue_info{};
    queue_info.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
    queue_info.queueFamilyIndex = graphics_queue_family;
    queue_info.queueCount = 1;
    queue_info.pQueuePriorities = &queue_priority;

    VkPhysicalDeviceVulkan13Features vulkan_13_features{};
    vulkan_13_features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES;
    vulkan_13_features.dynamicRendering = VK_TRUE;
    vulkan_13_features.shaderDemoteToHelperInvocation = VK_TRUE;

    VkDeviceCreateInfo device_info{};
    device_info.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    device_info.pNext = &vulkan_13_features;
    device_info.queueCreateInfoCount = 1;
    device_info.pQueueCreateInfos = &queue_info;
    VkPhysicalDeviceFeatures features{};
    vkGetPhysicalDeviceFeatures(physical_device,&features);
    if(!features.fragmentStoresAndAtomics){
        throw std::runtime_error("Exact transparency requires fragmentStoresAndAtomics");
    }
    VkPhysicalDeviceProperties properties{};
    vkGetPhysicalDeviceProperties(physical_device,&properties);
    if(properties.limits.maxBoundDescriptorSets < 6){
        throw std::runtime_error("The camera/light/material/G-buffer/OIT layout requires six descriptor sets including skybox");
    }
    VkPhysicalDeviceFeatures enabled_features{};
    enabled_features.fragmentStoresAndAtomics = VK_TRUE;
    enabled_features.shaderInt64 = VK_TRUE;
    VkPhysicalDeviceFragmentShaderInterlockFeaturesEXT interlock{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FRAGMENT_SHADER_INTERLOCK_FEATURES_EXT};
    VkPhysicalDeviceShaderImageAtomicInt64FeaturesEXT image64{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_IMAGE_ATOMIC_INT64_FEATURES_EXT};
    interlock.pNext = &image64;
    VkPhysicalDeviceFeatures2 supported{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2};
    supported.pNext = &interlock;
    vkGetPhysicalDeviceFeatures2(physical_device,&supported);
    if(!features.shaderInt64 || !interlock.fragmentShaderPixelInterlock || !image64.shaderImageInt64Atomics){
        throw std::runtime_error("16-layer OIT heap requires shaderInt64, pixel interlock and 64-bit storage images");
    }
    interlock.fragmentShaderSampleInterlock = VK_FALSE;
    interlock.fragmentShaderShadingRateInterlock = VK_FALSE;
    image64.sparseImageInt64Atomics = VK_FALSE;
    vulkan_13_features.pNext = &interlock;
    std::vector<const char*> extensions{VK_EXT_FRAGMENT_SHADER_INTERLOCK_EXTENSION_NAME,VK_EXT_SHADER_IMAGE_ATOMIC_INT64_EXTENSION_NAME};
    if(context.surface)extensions.push_back(VK_KHR_SWAPCHAIN_EXTENSION_NAME);
    device_info.enabledExtensionCount = uint32_t(extensions.size());
    device_info.ppEnabledExtensionNames = extensions.data();
    enabled_features.textureCompressionBC = features.textureCompressionBC;
    device_info.pEnabledFeatures = &enabled_features;

    if(vkCreateDevice(physical_device,&device_info,nullptr,&device) != VK_SUCCESS){
        throw std::runtime_error("Failed to create Vulkan device");
    }
}

void render_system::init_pl(){
    material::create_layout(device);
    //pipeline descriptor set layout
    VkDescriptorSetLayout set_layouts[6]{
        res.cam.camera_layout,//set 0 camera
        res.light.light_layout,//set 1 light
        material::material_layout,//set 2 material
        res.cam.g_buffer_layout,//set 3 g-buffer
        res.oit.layout,//set 4 OIT capture
        res.sky.get_layout()//set 5 atmosphere
    };

    //transform model matrix push constant
    VkPushConstantRange push_constant_range{};
    push_constant_range.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
    push_constant_range.offset = 0;
    push_constant_range.size = sizeof(glm::mat4);

    //创建几何 pipeline layout
    VkPipelineLayoutCreateInfo layout_info{};
    layout_info.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    layout_info.setLayoutCount = 6;
    layout_info.pSetLayouts = set_layouts;
    VkPushConstantRange push_ranges[]{push_constant_range,{VK_SHADER_STAGE_FRAGMENT_BIT,sizeof(glm::mat4),sizeof(uint32_t)}};
    layout_info.pushConstantRangeCount = 2;
    layout_info.pPushConstantRanges = push_ranges;

    if(vkCreatePipelineLayout(device,&layout_info,nullptr,&geometry_pipeline_layout) != VK_SUCCESS){
        throw std::runtime_error("Failed to create opaque/OIT capture pipeline layout");
    }

    VkPipelineLayoutCreateInfo oit_layout_info{VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
    oit_layout_info.setLayoutCount = 1;
    oit_layout_info.pSetLayouts = &res.oit.layout;
    if(vkCreatePipelineLayout(device,&oit_layout_info,nullptr,&oit_pipeline_layout) != VK_SUCCESS){
        throw std::runtime_error("Failed to create OIT resolve pipeline layout");
    }

    struct module_scope{
        VkDevice device;
        std::vector<VkShaderModule> modules;
        ~module_scope(){
            for(VkShaderModule module : modules) vkDestroyShaderModule(device,module,nullptr);
        }
    } loaded{device,{}};
    auto load_module = [&](const char* name){
        auto path = shader_root / name;
        const std::string file=name;
        const char* key=file.starts_with("skybox/") ? "skybox" : file.starts_with("post_processing/") ? "post_processing" : file.starts_with("opaque/") ? "opaque_rendering" : file.starts_with("pre_processing/") ? "pre_processing" :
            file.find("capture/")!=std::string::npos ? "transparent_capture" : file.find("sort/")!=std::string::npos ? "transparent_sort" : "transparent_resolve";
        const auto stage=file.ends_with(".vert.spv") ? shader_stage::vertex : file.ends_with(".frag.spv") ? shader_stage::fragment : shader_stage::compute;
        auto& fs=egn.get_file_system();auto& configs=fs.get_resources().get_render_pass_data();
        for(const auto& [guid,info]:fs.get_files())if(info.type==file_type::render_pass){
            const auto* pass=configs.get_pass(guid);if(!pass || pass->builtin!=key)continue;
            if(const auto* shader=configs.get_shader(pass->shader_guid))for(const auto& sub_guid:shader->sub_shader_guids)
                if(const auto* sub=configs.get_sub_shader(sub_guid);sub && sub->stage==stage){path=fs.get_scan().get_library_path(sub_guid);break;}
        }
        std::ifstream input(path,std::ios::binary | std::ios::ate);
        if(!input) throw std::runtime_error("Missing built-in SPIR-V: " + path.string());
        const size_t bytes = static_cast<size_t>(input.tellg());
        std::vector<uint32_t> words(bytes / sizeof(uint32_t));
        input.seekg(0);
        input.read(reinterpret_cast<char*>(words.data()),words.size() * sizeof(uint32_t));
        VkShaderModuleCreateInfo module_info{VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO};
        module_info.codeSize = words.size() * sizeof(uint32_t);
        module_info.pCode = words.data();
        VkShaderModule module = VK_NULL_HANDLE;
        if(vkCreateShaderModule(device,&module_info,nullptr,&module) != VK_SUCCESS){
            throw std::runtime_error("Failed to create built-in shader module");
        }
        loaded.modules.push_back(module);
        return module;
    };
    std::vector<VkPipelineShaderStageCreateInfo> stages(2);
    for(auto& stage : stages){
        stage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        stage.pName = "main";
    }
    stages[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
    stages[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    stages[0].module = load_module("opaque/opaque.vert.spv");
    stages[1].module = load_module("opaque/opaque.frag.spv");
    geometry_pipeline = create_pipeline(stages);
    stages[0].module=load_module("skybox/skybox.vert.spv");stages[1].module=load_module("skybox/skybox.frag.spv");
    skybox_pipeline=create_pipeline(stages,pipeline_pass::skybox);
    stages[0].module=load_module("post_processing/tone_mapping.vert.spv");stages[1].module=load_module("post_processing/tone_mapping.frag.spv");
    tone_mapping_pipeline=create_pipeline(stages,pipeline_pass::tone_mapping);
    stages[0].module = load_module("transparent_rendering/capture/capture.vert.spv");
    stages[1].module = load_module("transparent_rendering/capture/capture.frag.spv");
    transparent_pipeline = create_pipeline(stages,pipeline_pass::transparent_capture);
    stages[0].module = load_module("transparent_rendering/resolve/resolve.vert.spv");
    stages[1].module = load_module("transparent_rendering/resolve/resolve.frag.spv");
    oit_resolve_pipeline = create_pipeline(stages,pipeline_pass::transparent_resolve);

    VkPushConstantRange cull_push{VK_SHADER_STAGE_COMPUTE_BIT,0,sizeof(uint32_t)};
    VkPipelineLayoutCreateInfo cull_layout{VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
    cull_layout.setLayoutCount = 1; cull_layout.pSetLayouts = &culling.layout;
    cull_layout.pushConstantRangeCount = 1; cull_layout.pPushConstantRanges = &cull_push;
    if(vkCreatePipelineLayout(device,&cull_layout,nullptr,&cull_pipeline_layout) != VK_SUCCESS)
        throw std::runtime_error("Failed to create frustum culling pipeline layout");
    VkComputePipelineCreateInfo compute{VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO};
    compute.stage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    compute.stage.stage = VK_SHADER_STAGE_COMPUTE_BIT;
    compute.stage.module = load_module("pre_processing/frustum_cull.comp.spv");
    compute.stage.pName = "main"; compute.layout = cull_pipeline_layout;
    if(vkCreateComputePipelines(device,VK_NULL_HANDLE,1,&compute,nullptr,&cull_pipeline) != VK_SUCCESS)
        throw std::runtime_error("Failed to create frustum culling compute pipeline");
    compute.stage.module=load_module("transparent_rendering/sort/sort.comp.spv");
    compute.layout=oit_pipeline_layout;
    if(vkCreateComputePipelines(device,VK_NULL_HANDLE,1,&compute,nullptr,&oit_sort_pipeline)!=VK_SUCCESS)
        throw std::runtime_error("Failed to create transparent sorting pipeline");
}

VkPipeline render_system::create_pipeline(const std::vector<VkPipelineShaderStageCreateInfo>& stages, pipeline_pass pass){
    const bool capture = pass == pipeline_pass::transparent_capture;
    const bool resolve = pass == pipeline_pass::transparent_resolve;
    const bool sky=pass==pipeline_pass::skybox,tone=pass==pipeline_pass::tone_mapping;
    const bool fullscreen=resolve || sky || tone;
    bool has_tessellation = false;
    for(const auto& stage : stages){
        has_tessellation |= stage.stage == VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT
                         || stage.stage == VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT;
    }

    //vertex input
    VkVertexInputBindingDescription binding{};
    binding.binding = 0;
    binding.stride = sizeof(vertex);
    binding.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

    const VkVertexInputAttributeDescription attributes[]{
        {0,0,VK_FORMAT_R32G32B32_SFLOAT,offsetof(vertex,position)},
        {1,0,VK_FORMAT_R32G32_SFLOAT,offsetof(vertex,uv)},
        {2,0,VK_FORMAT_R32G32B32_SFLOAT,offsetof(vertex,normal)}
    };

    VkPipelineVertexInputStateCreateInfo vertex_input{};
    vertex_input.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vertex_input.vertexBindingDescriptionCount = fullscreen ? 0 : 1;
    vertex_input.pVertexBindingDescriptions = &binding;
    vertex_input.vertexAttributeDescriptionCount = fullscreen ? 0 : 3;
    vertex_input.pVertexAttributeDescriptions = attributes;

    //input assembly
    VkPipelineInputAssemblyStateCreateInfo assembly{};
    assembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    assembly.topology = has_tessellation ? VK_PRIMITIVE_TOPOLOGY_PATCH_LIST : VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    assembly.primitiveRestartEnable = VK_FALSE;

    //viewport
    VkPipelineViewportStateCreateInfo viewport{};
    viewport.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewport.viewportCount = 1;
    viewport.scissorCount = 1;

    //rasterization
    VkPipelineRasterizationStateCreateInfo rasterization{};
    rasterization.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rasterization.depthClampEnable = VK_FALSE;
    rasterization.rasterizerDiscardEnable = VK_FALSE;
    rasterization.polygonMode = VK_POLYGON_MODE_FILL;
    rasterization.cullMode = VK_CULL_MODE_NONE;
    rasterization.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    rasterization.depthBiasEnable = VK_FALSE;
    rasterization.lineWidth = 1.0f;

    //MSAA
    VkPipelineMultisampleStateCreateInfo multisample{};
    multisample.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisample.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
    multisample.sampleShadingEnable = VK_FALSE;

    //depth
    VkPipelineDepthStencilStateCreateInfo depth_stencil{};
    depth_stencil.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    depth_stencil.depthTestEnable = !resolve && !tone;
    depth_stencil.depthWriteEnable = !capture && !fullscreen;
    depth_stencil.depthCompareOp = (capture || sky) ? VK_COMPARE_OP_LESS_OR_EQUAL : VK_COMPARE_OP_LESS;
    depth_stencil.depthBoundsTestEnable = VK_FALSE;
    depth_stencil.stencilTestEnable = VK_FALSE;

    //color + normal attachment
    VkPipelineColorBlendAttachmentState blend_attachments[2]{};

    for(auto& attachment : blend_attachments){
        attachment.blendEnable = VK_FALSE;
        attachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    }
    if(resolve || capture){
        auto& color = blend_attachments[0];
        color.blendEnable = VK_TRUE;
        color.srcColorBlendFactor = capture ? VK_BLEND_FACTOR_SRC_ALPHA : VK_BLEND_FACTOR_ONE;
        color.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
        color.colorBlendOp = VK_BLEND_OP_ADD;
        color.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
        color.dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
        color.alphaBlendOp = VK_BLEND_OP_ADD;
    }

    VkPipelineColorBlendStateCreateInfo blend{};
    blend.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    blend.logicOpEnable = VK_FALSE;
    blend.attachmentCount = (capture || resolve || tone) ? 1 : 2;
    blend.pAttachments = blend_attachments;

    //dynamic states
    const VkDynamicState dynamic_states[]{
        VK_DYNAMIC_STATE_VIEWPORT,
        VK_DYNAMIC_STATE_SCISSOR
    };

    VkPipelineDynamicStateCreateInfo dynamic{};
    dynamic.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    dynamic.dynamicStateCount = 2;
    dynamic.pDynamicStates = dynamic_states;

    //dynamic rendering attachment formats
    const VkFormat color_formats[]{
        tone ? VK_FORMAT_R8G8B8A8_UNORM : res.cam.g_buffer_format,//color
        res.cam.g_buffer_format//normal
    };

    VkPipelineRenderingCreateInfo rendering{};
    rendering.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO;
    rendering.colorAttachmentCount = blend.attachmentCount;
    rendering.pColorAttachmentFormats = color_formats;
    rendering.depthAttachmentFormat = (resolve || tone) ? VK_FORMAT_UNDEFINED : res.cam.depth_format;

    VkPipelineTessellationStateCreateInfo tessellation{};
    tessellation.sType = VK_STRUCTURE_TYPE_PIPELINE_TESSELLATION_STATE_CREATE_INFO;
    tessellation.patchControlPoints = patch_control_points;

    //graphics pipeline
    VkGraphicsPipelineCreateInfo pipeline_info{};
    pipeline_info.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipeline_info.pNext = &rendering;
    pipeline_info.stageCount = static_cast<uint32_t>(stages.size());
    pipeline_info.pStages = stages.data();
    pipeline_info.pVertexInputState = &vertex_input;
    pipeline_info.pInputAssemblyState = &assembly;
    pipeline_info.pTessellationState = has_tessellation ? &tessellation : nullptr;
    pipeline_info.pViewportState = &viewport;
    pipeline_info.pRasterizationState = &rasterization;
    pipeline_info.pMultisampleState = &multisample;
    pipeline_info.pDepthStencilState = &depth_stencil;
    pipeline_info.pColorBlendState = &blend;
    pipeline_info.pDynamicState = &dynamic;
    pipeline_info.layout = resolve ? oit_pipeline_layout : geometry_pipeline_layout;

    VkPipeline pipeline = VK_NULL_HANDLE;
    if(vkCreateGraphicsPipelines(device,VK_NULL_HANDLE,1,&pipeline_info,nullptr,&pipeline) != VK_SUCCESS){
        vkDestroyPipeline(device,pipeline,nullptr);
        throw std::runtime_error("Failed to create opaque/OIT graphics pipeline");
    }
    return pipeline;
}
void render_system::init(const std::filesystem::path& path,const render_window_info& window){
    shader_root=path;
    if(device != VK_NULL_HANDLE) return;
    init_dev(window);
    res.init(physical_device,device,context.queue_family);
    context.instance=instance;context.physical_device=physical_device;context.device=device;context.queue=res.graphics_queue;
    res.oit.init(device,res.cam.extent);
    res.oit.create(physical_device);
    culling.init(physical_device,device);
    init_pl();
}

render_context render_system::get_context() const noexcept{return context;}
render_image render_system::get_color_image() const noexcept{return {res.cam.display_view,res.cam.color_sampler,res.cam.extent};}
void render_system::shutdown() noexcept{cleanup();context={};}
void render_system::resize_viewport(VkExtent2D size){
    if(size.width==res.cam.extent.width && size.height==res.cam.extent.height)return;
    vkDeviceWaitIdle(device);
    auto& cam=scene.get_main_camera();cam.extent=size;cam.aspect=float(size.width)/size.height;
    cam.update_projection();cam.update_view();
    res.cam.resize(physical_device,size);
    res.oit.extent=size;res.oit.update(physical_device);
    attachments_initialized=false;
}
render_system::~render_system(){
    cleanup();
}

void render_system::cleanup() noexcept{
    if(device == VK_NULL_HANDLE){
        if(instance != VK_NULL_HANDLE) vkDestroyInstance(instance,nullptr);
        instance = VK_NULL_HANDLE;
        return;
    }
    vkDeviceWaitIdle(device);
    vkDestroyPipeline(device,cull_pipeline,nullptr);
    vkDestroyPipelineLayout(device,cull_pipeline_layout,nullptr);
    cull_pipeline = VK_NULL_HANDLE; cull_pipeline_layout = VK_NULL_HANDLE;
    culling.cleanup();
    vkDestroyPipeline(res.device,transparent_pipeline,nullptr);
    vkDestroyPipeline(res.device,oit_resolve_pipeline,nullptr);
    vkDestroyPipeline(res.device,oit_sort_pipeline,nullptr);
    oit_sort_pipeline=VK_NULL_HANDLE;
    vkDestroyPipelineLayout(res.device,oit_pipeline_layout,nullptr);
    transparent_pipeline = VK_NULL_HANDLE;
    oit_resolve_pipeline = VK_NULL_HANDLE;
    oit_pipeline_layout = VK_NULL_HANDLE;
    vkDestroyPipeline(res.device,skybox_pipeline,nullptr);vkDestroyPipeline(res.device,tone_mapping_pipeline,nullptr);
    skybox_pipeline=tone_mapping_pipeline=VK_NULL_HANDLE;
    vkDestroyPipeline(res.device, geometry_pipeline, nullptr);
    vkDestroyPipelineLayout(res.device, geometry_pipeline_layout, nullptr);
    geometry_pipeline = VK_NULL_HANDLE;
    geometry_pipeline_layout = VK_NULL_HANDLE;
    material::destroy_layout(device);
    res.cleanup();
    vkDestroyDevice(device,nullptr);
    vkDestroyInstance(instance,nullptr);
    device = VK_NULL_HANDLE;
    instance = VK_NULL_HANDLE;
}

void render_system::pass(){
    ++stream_frame;
    for(const auto& [guid,file]:egn.get_file_system().get_files())if(file.type==file_type::render_pass){
        const auto* pass=egn.get_file_system().get_resources().get_render_pass_data().get_pass(guid);
        if(pass && pass->builtin=="pre_processing")culling_enabled=pass->enabled;
        if(pass && pass->builtin=="skybox")skybox_enabled=pass->enabled;
        if(pass && pass->builtin=="post_processing")tone_mapping_enabled=pass->enabled;
    }
    prim_cache.poll();
    scene.update();
    res.update();
    for(auto* obj:scene.get_object_manger().objects)if(obj->mat)obj->mat->prepare(physical_device,device,res.command_pool,res.graphics_queue);
    prepare_culling();
    VkCommandBuffer command_buffer = VK_NULL_HANDLE;
    VkCommandBufferAllocateInfo allocate{VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
    allocate.commandPool = res.command_pool;
    allocate.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocate.commandBufferCount = 1;
    vkAllocateCommandBuffers(res.device,&allocate,&command_buffer);
    VkCommandBufferBeginInfo begin{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
    begin.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    vkBeginCommandBuffer(command_buffer,&begin);

    pre_processing_pass(command_buffer);
    geometry_pass(command_buffer);
    transparent_rendering(command_buffer);
    transition_attachments(command_buffer,false);
    post_processing_pass(command_buffer);

    vkEndCommandBuffer(command_buffer);
    VkSubmitInfo submit{VK_STRUCTURE_TYPE_SUBMIT_INFO};
    submit.commandBufferCount = 1;
    submit.pCommandBuffers = &command_buffer;
    vkQueueSubmit(res.graphics_queue,1,&submit,VK_NULL_HANDLE);
    vkQueueWaitIdle(res.graphics_queue);
    for(auto* obj : scene.get_object_manger().objects) if(obj->mesh) obj->mesh->finish_stream();
    prim_cache.trim(stream_frame>2 ? stream_frame-2 : 0);
    vkFreeCommandBuffers(res.device,res.command_pool,1,&command_buffer);
}
void render_system::opaque_rendering(VkCommandBuffer command_buffer){
    vkCmdBindPipeline(command_buffer,VK_PIPELINE_BIND_POINT_GRAPHICS,geometry_pipeline);
    for(const auto& draw : opaque_draws){
        object* obj = draw.obj;
        obj->upload(command_buffer,geometry_pipeline_layout);
        obj->mat->upload(command_buffer,geometry_pipeline_layout,draw.sub.mat_idx);
        vkCmdDrawIndexedIndirect(command_buffer,culling.indirect_buffer(),VkDeviceSize(draw.command_index)*sizeof(VkDrawIndexedIndirectCommand),1,sizeof(VkDrawIndexedIndirectCommand));
    }
}

void render_system::transparent_rendering(VkCommandBuffer command_buffer){
    res.oit.extent = res.cam.extent;
    res.oit.update(physical_device);
    res.oit.upload(command_buffer,geometry_pipeline_layout);

    if(transparent_draws.empty()) return;

    // 读取不透明深度，堆外颜色直接混合到已有颜色附件。
    VkMemoryBarrier attachment_barrier{VK_STRUCTURE_TYPE_MEMORY_BARRIER};
    attachment_barrier.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
    attachment_barrier.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT |
                                       VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT;
    const VkPipelineStageFlags attachment_stages = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT |
        VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
    vkCmdPipelineBarrier(command_buffer,attachment_stages,attachment_stages,0,1,&attachment_barrier,0,nullptr,0,nullptr);

    VkRenderingAttachmentInfo color{VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO};
    color.imageView = res.cam.color_view;
    color.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    color.loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
    color.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    VkRenderingAttachmentInfo depth{VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO};
    depth.imageView = res.cam.depth_view;
    depth.imageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
    depth.loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
    depth.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    VkRenderingInfo capture{VK_STRUCTURE_TYPE_RENDERING_INFO};
    capture.renderArea.extent = res.cam.extent;
    capture.layerCount = 1;
    capture.colorAttachmentCount = 1;
    capture.pColorAttachments = &color;
    capture.pDepthAttachment = &depth;
    vkCmdBeginRendering(command_buffer,&capture);
    vkCmdBindPipeline(command_buffer,VK_PIPELINE_BIND_POINT_GRAPHICS,transparent_pipeline);
    for(const auto& draw : transparent_draws){
        object* obj = draw.obj;
        obj->upload(command_buffer,geometry_pipeline_layout);
        obj->mat->upload(command_buffer,geometry_pipeline_layout,draw.sub.mat_idx);
        vkCmdDrawIndexedIndirect(command_buffer,culling.indirect_buffer(),VkDeviceSize(draw.command_index)*sizeof(VkDrawIndexedIndirectCommand),1,sizeof(VkDrawIndexedIndirectCommand));
    }
    vkCmdEndRendering(command_buffer);

    VkMemoryBarrier sorted{VK_STRUCTURE_TYPE_MEMORY_BARRIER};
    sorted.srcAccessMask=VK_ACCESS_SHADER_WRITE_BIT;
    sorted.dstAccessMask=VK_ACCESS_SHADER_READ_BIT|VK_ACCESS_SHADER_WRITE_BIT;
    vkCmdPipelineBarrier(command_buffer,VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,0,1,&sorted,0,nullptr,0,nullptr);
    vkCmdBindPipeline(command_buffer,VK_PIPELINE_BIND_POINT_COMPUTE,oit_sort_pipeline);
    vkCmdBindDescriptorSets(command_buffer,VK_PIPELINE_BIND_POINT_COMPUTE,oit_pipeline_layout,0,1,&res.oit.descriptor_set,0,nullptr);
    vkCmdDispatch(command_buffer,(res.cam.extent.width+7)/8,(res.cam.extent.height+7)/8,1);
    sorted.srcAccessMask=VK_ACCESS_SHADER_WRITE_BIT|VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    sorted.dstAccessMask=VK_ACCESS_SHADER_READ_BIT|VK_ACCESS_COLOR_ATTACHMENT_READ_BIT|VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    vkCmdPipelineBarrier(command_buffer,VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT|VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT|VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
        VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT|VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,0,1,&sorted,0,nullptr,0,nullptr);
    capture.pDepthAttachment=nullptr;
    vkCmdBeginRendering(command_buffer,&capture);
    vkCmdBindPipeline(command_buffer,VK_PIPELINE_BIND_POINT_GRAPHICS,oit_resolve_pipeline);
    vkCmdBindDescriptorSets(command_buffer,VK_PIPELINE_BIND_POINT_GRAPHICS,oit_pipeline_layout,0,1,&res.oit.descriptor_set,0,nullptr);
    vkCmdDraw(command_buffer,3,1,0,0);
    vkCmdEndRendering(command_buffer);
}

void render_system::transition_attachments(VkCommandBuffer command_buffer, bool for_rendering){
    VkImageMemoryBarrier barriers[3]{};
    const VkImage images[]{res.cam.color_image,res.cam.normal_image,res.cam.depth_image};
    for(uint32_t i = 0; i < 3; ++i){
        const bool depth = i == 2;
        auto& barrier = barriers[i];
        barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        barrier.image = images[i];
        barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.subresourceRange.aspectMask = depth ? VK_IMAGE_ASPECT_DEPTH_BIT : VK_IMAGE_ASPECT_COLOR_BIT;
        if(depth && (res.cam.depth_format == VK_FORMAT_D24_UNORM_S8_UINT ||
                     res.cam.depth_format == VK_FORMAT_D32_SFLOAT_S8_UINT ||
                     res.cam.depth_format == VK_FORMAT_D16_UNORM_S8_UINT)){
            barrier.subresourceRange.aspectMask |= VK_IMAGE_ASPECT_STENCIL_BIT;
        }
        barrier.subresourceRange.levelCount = 1;
        barrier.subresourceRange.layerCount = 1;
        const VkImageLayout attachment = depth ? VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL : VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        const VkImageLayout sampled = depth ? VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL : VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        const VkAccessFlags access = depth ? VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT
                                          : VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
        barrier.oldLayout = for_rendering ? (attachments_initialized ? sampled : VK_IMAGE_LAYOUT_UNDEFINED) : attachment;
        barrier.newLayout = for_rendering ? attachment : sampled;
        barrier.srcAccessMask = for_rendering ? (attachments_initialized ? VK_ACCESS_SHADER_READ_BIT : 0) : access;
        barrier.dstAccessMask = for_rendering ? access : VK_ACCESS_SHADER_READ_BIT;
    }
    const VkPipelineStageFlags attachment_stages = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT |
        VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
    const VkPipelineStageFlags source = for_rendering ? (attachments_initialized ? VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT : VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT)
                                                     : attachment_stages;
    const VkPipelineStageFlags destination = for_rendering ? attachment_stages : VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
    vkCmdPipelineBarrier(command_buffer,source,destination,0,0,nullptr,0,nullptr,3,barriers);
    if(!for_rendering) attachments_initialized = true;
}

void render_system::geometry_pass(VkCommandBuffer command_buffer){
    //切换颜色、法线和深度贴图到渲染状态
    transition_attachments(command_buffer,true);

    //设置颜色和法线 attachment
    VkRenderingAttachmentInfo color_attachments[2]{};
    color_attachments[0].sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
    color_attachments[0].imageView = res.cam.color_view;
    color_attachments[0].imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    color_attachments[0].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    color_attachments[0].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    color_attachments[0].clearValue.color = {{0.055f,0.069f,0.09f,1.0f}};

    color_attachments[1].sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
    color_attachments[1].imageView = res.cam.normal_view;
    color_attachments[1].imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    color_attachments[1].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    color_attachments[1].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    color_attachments[1].clearValue.color = {{0.0f,0.0f,0.0f,0.0f}};

    //设置深度 attachment
    VkRenderingAttachmentInfo depth_attachment{};
    depth_attachment.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
    depth_attachment.imageView = res.cam.depth_view;
    depth_attachment.imageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
    depth_attachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    depth_attachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    depth_attachment.clearValue.depthStencil = {1.0f,0};

    //设置 dynamic rendering
    VkRenderingInfo rendering_info{};
    rendering_info.sType = VK_STRUCTURE_TYPE_RENDERING_INFO;
    rendering_info.renderArea.offset = {0,0};
    rendering_info.renderArea.extent = res.cam.extent;
    rendering_info.layerCount = 1;
    rendering_info.colorAttachmentCount = 2;
    rendering_info.pColorAttachments = color_attachments;
    rendering_info.pDepthAttachment = &depth_attachment;

    //开始几何渲染
    vkCmdBeginRendering(command_buffer,&rendering_info);

    //绑定几何管线
    vkCmdBindPipeline(command_buffer,VK_PIPELINE_BIND_POINT_GRAPHICS,geometry_pipeline);

    //设置 viewport
    VkViewport viewport{};
    viewport.x = 0.0f;
    viewport.y = 0.0f;
    viewport.width = static_cast<float>(res.cam.extent.width);
    viewport.height = static_cast<float>(res.cam.extent.height);
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;

    vkCmdSetViewport(command_buffer,0,1,&viewport);

    //设置 scissor
    VkRect2D scissor{};
    scissor.offset = {0,0};
    scissor.extent = res.cam.extent;

    vkCmdSetScissor(command_buffer,0,1,&scissor);

    //绑定 camera/light descriptor set
    res.upload(command_buffer,geometry_pipeline_layout);

    opaque_rendering(command_buffer);
    if(skybox_enabled){vkCmdBindPipeline(command_buffer,VK_PIPELINE_BIND_POINT_GRAPHICS,skybox_pipeline);vkCmdDraw(command_buffer,3,1,0,0);}
    vkCmdEndRendering(command_buffer);
}

void render_system::prepare_culling(){
    opaque_draws.clear();transparent_draws.clear();
    std::vector<prim_cull_item> candidates;
    std::vector<VkDrawIndexedIndirectCommand> commands;
    std::unordered_map<uint32_t,object*> objects;
    for(auto* obj:scene.get_object_manger().objects) objects.emplace(obj->run_id,obj);
    const auto& cam=scene.get_main_camera();
    float radius=0;for(float width:camera::dis)radius+=width;
    const auto p=cam.get_position();
    const auto selected=prim_table.query(AABB(p-glm::vec3(radius),p+glm::vec3(radius)));
    std::unordered_set<uint32_t> needed;
    for(const auto& instance:selected){
        object* obj=objects.at(instance.id);
        if(!obj->mesh || !obj->mat || !obj->tran)continue;
        needed.insert(instance.id);
        obj->mesh->update_stream(cam,instance.bounds,prim_cache,stream_frame);
        if(!obj->mesh->drawable || !obj->mesh->get_vertex_buffer() || !obj->mesh->get_index_buffer())continue;
        const auto& lo=instance.bounds.get_min();const auto& hi=instance.bounds.get_max();
        const int level=obj->mesh->get_cur_lod();
        for(const auto& sub:obj->mesh->get_data().sub_meshes){
            if(!sub.index_count)continue;
            if(sub.mat_idx>=obj->mat->get_sub_materials().size())continue;
            if(!obj->mat->get_sub_material(sub.mat_idx)->visible)continue;
            const auto& material=obj->mat->get_sub_material(sub.mat_idx)->uniform_data;
            const auto mode=material.texture_flags_2.z;
            const bool transparent=obj->is_transparent || mode==3 || (mode==0 && material.base_color.a<1);
            auto& draws=transparent ? transparent_draws : opaque_draws;
            const auto index=uint32_t(commands.size());prim_cull_item item;
            item.bounds_min={lo.x,lo.y,lo.z,0};item.bounds_max={hi.x,hi.y,hi.z,0};
            item.instance_index=obj->object_idx;item.lod=level<0 ? 0 : uint32_t(level);item.transparent=transparent;
            candidates.push_back(item);commands.push_back({sub.index_count,culling_enabled ? 0u : 1u,sub.front_idx,sub.vertex_off,0});
            draws.push_back({obj,sub,index});
        }
    }
    for(auto* obj:scene.get_object_manger().objects)if(obj->mesh && needed.find(obj->run_id)==needed.end())obj->mesh->release_stream();
    culling.prepare(candidates,commands,res.cam.uniform_data.vp);
}
void render_system::pre_processing_pass(VkCommandBuffer command_buffer){
    for(auto* obj : scene.get_object_manger().objects) if(obj->mesh) obj->mesh->stream_upload(command_buffer,prim_cache);
    VkMemoryBarrier geometry_ready{VK_STRUCTURE_TYPE_MEMORY_BARRIER};
    geometry_ready.srcAccessMask=VK_ACCESS_TRANSFER_WRITE_BIT;
    geometry_ready.dstAccessMask=VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT|VK_ACCESS_INDEX_READ_BIT;
    vkCmdPipelineBarrier(command_buffer,VK_PIPELINE_STAGE_TRANSFER_BIT,VK_PIPELINE_STAGE_VERTEX_INPUT_BIT,0,1,&geometry_ready,0,nullptr,0,nullptr);
    if(culling_enabled)culling.dispatch(command_buffer,cull_pipeline,cull_pipeline_layout);
    else{
        VkMemoryBarrier ready{VK_STRUCTURE_TYPE_MEMORY_BARRIER};ready.srcAccessMask=VK_ACCESS_HOST_WRITE_BIT;ready.dstAccessMask=VK_ACCESS_INDIRECT_COMMAND_READ_BIT;
        vkCmdPipelineBarrier(command_buffer,VK_PIPELINE_STAGE_HOST_BIT,VK_PIPELINE_STAGE_DRAW_INDIRECT_BIT,0,1,&ready,0,nullptr,0,nullptr);
    }
}
}

