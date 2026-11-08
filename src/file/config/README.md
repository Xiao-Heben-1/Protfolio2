# 场景、对象与组件配置

配置声明放hpp，读写实现放对应cpp。配置只保存CPU数据和持久化GUID。

```text
config/
  scene_config.hpp/.cpp        场景公共信息、主相机、灯光列表、对象列表
  object_config.hpp/.cpp       对象公共信息、透明类别、transform/mesh/material
  workspace_config.hpp/.cpp    工作空间、library、内置shader、编译器和include根路径
  skybox_config.hpp/.cpp       大气、地表、曝光、积分采样数；随 scene 保存
  render_pass/
    pass_config.hpp/.cpp       pass阶段、shader GUID、顺序、开关和图形管线状态
    shader_config.hpp/.cpp     sub_shader GUID列表
    sub_shader_config.hpp/.cpp 源码GUID、着色阶段、入口函数、宏和include路径
  component/
    transform_config.hpp/.cpp  位置、欧拉角（弧度）、缩放
    mesh_config.hpp/.cpp       模型源GUID、LOD0/1/2 GUID、可选局部AABB、builtin_geometry
    material_config.hpp/.cpp   材质槽GUID和逐槽覆盖参数
    camera_config.hpp/.cpp     位姿、投影、四段距离、分辨率、附件格式
    light_config.hpp/.cpp      灯光GUID/名称/类型、位姿、颜色、强度和衰减
  config_json.hpp/.cpp         cpp共用的基础JSON读写
```

scene_config继承file_config的guid、name、version。version表示配置格式，几何资源仍没有资源版本字段。object_config可以嵌入场景，也可作为独立.object资源；组件字段不再定义在场景文件中。

新.scene和.object文件第一行是GUID，其后是JSON。嵌入场景的对象和灯光在自己的JSON对象中保存guid。首次创建配置生成GUID，保存/重载沿用它；移除.guid侧车文件后，扫描器仍能从配置首行恢复同一标识。

对象JSON包含transform，以及可选mesh/material/camera、behavior标记和lights列表。mesh保存source_guid、三项lod_guids和可选bounds（依次为min.xyz、max.xyz）。内置几何另存builtin_geometry类型，Library清空后由object_loader重新生成。没有LOD时只填写lod_guids[0]；bounds缺省表示未提供局部包围盒。运行时cur_lod、run_id、世界AABB、缓存页状态和GPU句柄不写入配置。

material保存按槽位排列的guids和overrides。每项覆盖可设置PBR参数、shader_guid、贴图GUID映射、alpha_mode和alpha_cutoff。未填写的参数表示继承资源；显式填写0或空字符串会保留。alpha_mode对应运行时材质：0自动、1不透明、2裁剪、3混合。

camera.dis是四段宽度，默认128/512/512/1024；camera_config只保存值，LOD计算仍由mesh负责。g_buffer_format/depth_format保存VkFormat数值，不保存设备句柄。

通过file_scan::save_scene和save_object写入文件并刷新缓存；通过file_resources::get_scene_data/get_object_data读取缓存。独立对象的源模型、LOD GUID、AABB和透明类别同步登记到file_info。读取配置不会创建运行时对象或分配GPU资源。

兼容旧版纯JSON场景以及uid、IsTransparent、mesh_uid、material_uids、material_overrides、shader_uid字段，重新保存时写出新结构。旧字段缺失时沿用各组件默认值。

资源集成测试覆盖场景和独立对象的字段往返、缓存更新、GUID首行恢复、旧格式迁移，以及删除对象配置时的缓存注销。

## Pass、shader、sub_shader

层级为 pass → shader → 多个 sub_shader → 源码。三类资源分别使用 .pass、.shader、.sub_shader 后缀，首行是 GUID，后面是 JSON；读取和序列化在各自 cpp 中。通过 file_scan::save_pass/save_shader/save_sub_shader 保存，通过 file_resources::get_render_pass_data() 查询，外部修改缓存后再次调用对应 save 方法即可持久化。

pass.stage 可取 pre_processing、opaque、transparent_capture、transparent_sort、transparent_resolve、post_processing、geometry_pass、skybox。kind 为 graphics 或 compute；order 决定同一阶段内部顺序，enabled 决定是否进入 get_passes(stage) 的查询结果，同 order 按 GUID 排序。

图形状态包括 topology、polygon_mode、cull_mode、front_face、depth_test、depth_write、depth_compare 和按颜色附件顺序排列的 color_blends。每项 blend 保存颜色/alpha 的源目标因子、运算和写入掩码；这些 Vulkan 枚举按数值保存。compute 不使用图形状态。

shader.sub_shader_guids 保存各阶段配置的 GUID。sub_shader.stage 可取 vertex、fragment、geometry、tess_control、tess_evaluation、compute；source_guid 指向源码或现有 SPIR-V，entry_point 默认 main。defines 为宏名到宏值的映射，include_roots 为 source_root 相对路径或绝对路径，在全局 include 路径之前搜索。GLSL 入口遵循 glslc 的 main 约定，HLSL 可使用指定入口名。

scanner 按 sub_shader 的 GUID 生成 library_root/spirv/configured/<guid>/<guid>.<stage>.spv。源码、递归 include 内容、着色阶段、入口、宏和编译路径共同参与指纹；配置或工作空间内源码/include 修改后，在 file_system::update() 中刷新。没有变化则复用缓存；尚未找到 source_guid 时保留配置，等待源码导入。通过 get_library_path(sub_shader_guid) 获取编译结果路径。工作空间外的 include 目录不受目录监听，重启扫描时会重新核对指纹。

普通 .vert/.frag/.comp 等仍支持按扩展名直接编译默认变体；需要完全由 sub_shader 配置提供阶段及宏的共享源码可使用 .glsl，HLSL 可使用 .hlsl。配置编译结果与旧的按目录组合图形管线缓存分开管理。

当前这些属性已接入配置持久化、资源缓存和 shader 编译。内置管线启动时解析配置中的 shader/sub_shader 绑定，剔除开关逐帧读取。render_system 的阶段顺序仍由代码组织，自定义 pass 调度和运行时管线替换尚未实现。读取配置本身不会创建 GPU 对象。

## 工作空间与包路径

workspace_config 是启动配置，使用纯 JSON，不作为带 GUID 的资源扫描。仓库根目录的 workspace.example.json 可以直接按实际项目路径修改。CLI 接受 Render <workspace-directory> 或 Render <workspace.json>，宿主代码可调用：

```cpp
auto config = render::workspace_config::load("D:/Game/workspace.json");
render::egn.init(config);
render::egn.init_render(); // 从 config.builtin_shader_root 加载内置 SPIR-V
```

路径基准：

| 属性 | 相对路径的基准 | 默认值 |
| --- | --- | --- |
| workspace_root | workspace JSON 所在目录 | . |
| source_root | workspace_root | .（编辑器默认 src） |
| engine_root | workspace_root | 空；编辑器配置需指定引擎目录 |
| startup_scene | library_root | generated/scenes/default.scene |
| library_root | workspace_root | library |
| builtin_shader_root | library_root | builtin |
| shader_include_roots | workspace_root | shader/core/include、shader |
| shader_compiler | 带目录时相对 workspace_root；只有文件名时搜索 PATH | VULKAN_SDK/Bin/glslc.exe，未设置环境变量则 glslc.exe |

绝对路径直接使用。直接构造 workspace_config 传给 file_system/engine 时，workspace_root 以调用时工作目录为基准；load() 会以配置文件目录解析为绝对路径，之后改变工作目录不影响它。save() 按当前字段写出路径，load() 后保存会写入解析后的绝对路径。

用户源 file_info.path 相对 source_root；生成配置的 file_info.generated=true，path保存绝对路径。几何 file_info.path 和 library 缓存索引相对 library_root，调用 get_source_path/get_library_path 获得实际路径。源扫描与监听排除 library 子树；library/generated 单独建立派生资源索引，备份目录不作为源资源展示。每个工作空间使用自己的 library 缓存，启动会清理失去对应源文件的缓存。

CMake 提供 render::core 静态库目标用于底层测试/源码集成，以及 render::runtime 动态库目标，产物为 render.dll。独立 render_text 宿主通过导入库调用 src/front/editor_api.hpp 的 render_run_editor，模板在 tests/render_text_host。目前尚未添加 find_package 安装导出。

内置 SPIR-V 默认构建到 <build>/library/builtin，可通过 CMake 的 RENDER_SHADER_DIR 调整。部署时将该目录复制到宿主的 library_root/builtin，或直接用 builtin_shader_root 指向构建输出/包内只读 shader 目录。仓库示例指向 ../build/library/builtin（相对 library_root）。scanner 导入的 shader、贴图和几何则写入宿主 library_root；不需要修改包内 src 或在包源码旁生成构建侧车文件。

用户源资源只扫描 source_root；测试模型位于 render_text/src/model。
workspace.json 位于工作空间根目录。系统定义从 engine_root/config/system 读取，
生成到 library/generated；显式保存的项目覆盖放在 workspace_root/config，再覆盖系统默认值。
generate_system_config 根据定义内容指纹更新输出，save_project_config 保存用户覆盖，重建 Library 时可恢复。
内置 sub_shader 的 builtin_source 相对 engine_root/shader；未配置 engine_root 时兼容
builtin_shader_root 的同级 shader 目录。引擎源码不进入用户源资源索引。
Camera 配置保存 is_main；Object 加载时恢复主相机，camera::set_main() 保证唯一。
旧场景的独立 lights 由编辑器迁移为 Object 的 Light 组件，新的场景保存使用对象组件列表。
