# Render Editor

前端全部放在 `src/front`。`render.dll` 包含引擎和编辑器，导出 `editor_api.hpp`
中的 `render_run_editor`；`RenderEditor` 和独立的 `render_text` 都动态链接它。
`render::core` 静态目标用于底层测试及无编辑器集成，`Render` 保留资源导入入口。

## 构建和运行

```powershell
cmake -S . -B build -DRENDER_BUILD_EDITOR=ON
cmake --build build --target RenderEditor -j 4
.\build\RenderEditor.exe D:\YourWorkspace
```

RenderEditor 不传路径时读取 exe 同级 `workspace.json`；render_text 读取 build 上级的配置。
测试工作空间为 `D:/Project/render_text`，用户源文件只扫描 `src`，模型位于 `src/model`。
`library` 存放自动生成的配置、纹理、几何页、SPIR-V 和编辑器布局。
仓库的 `config/workspace.json` 是 RenderEditor 的启动模板。
也可以传入工作空间目录（包含 workspace.json）或配置 JSON。

```json
{
  "workspace_root": ".",
  "engine_root": "../render",
  "source_root": "src",
  "library_root": "library",
  "startup_scene": "generated/scenes/default.scene",
  "builtin_shader_root": "../../render/build/library/builtin",
  "shader_include_roots": ["src/shaders"]
}
```

引擎定义来自 `render/config/system`，内置 shader 源来自 `render/shader`，
配置用 `builtin_source` 引用。启动时将定义生成到 `library/generated` 并单独建立索引。
显式保存场景、Object 或 Pass 配置时，用户覆盖同时保存在工作空间根目录的 `config`；
删除 library 后可以由这些配置和源文件重新生成。系统配置不会写到用户 src。
作为包使用且不需要编辑器时，设 `RENDER_BUILD_EDITOR=OFF`。
首次启动创建 Main Camera、Sun（main_light）和 Cube；已有 startup_scene 则打开它。

独立测试宿主的模板在 `tests/render_text_host`；实际宿主位于 `D:/Project/render_text`：

```powershell
cmake -S D:\Project\render_text -B D:\Project\render_text\build -G "MinGW Makefiles"
cmake --build D:\Project\render_text\build -j 4
& D:\Project\render_text\build\render_text.exe
```

更新引擎后将新的 `build/render.dll` 复制到宿主 build。宿主只包含参数入口，
通过导入库 `librender.dll.a` 链接 DLL；引擎定义和 shader 仍从 engine_root 读取。

使用 `include/imgui-docking` 的官方 v1.89.8-docking 和 `include/glfw-source`
的 GLFW 3.4 源码，避免将 MSVC 的 GLFW `.lib` 链接到 MinGW。
原有 ImGui 目录保留；可通过 `RENDER_IMGUI_DIR` 指定其他兼容的 docking 源码。

## 面板和操作

- `load_model`：打开 FBX/OBJ 文件选择框，立即转换为 Object 加入场景。src 内模型直接引用，外部模型和贴图/MTL 依赖导入 `src/imported_models`；生成的 `.object` 写到 `library/generated/objects`。
- GameObject → Add Object 或 Hierarchy 的 `+ Object`：Empty Object；Geometry 下 Cube、Sphere、Capsule、Cylinder、Plane、Quad；Camera、Light 和 From Asset。
- Hierarchy：选择场景对象；Active Camera 是当前主相机的快捷入口。GameObject 菜单删除选中物体。
- Scene：显示实际 Vulkan 渲染结果；左键按世界 AABB 选物体。
- Inspector 的 Add Component 和顶栏 Component 菜单共用实际挂载操作，添加 Mesh / Material / Behavior / Camera / Main Light / Point Light / Directional Light；Transform 随 Object 创建。已存在的组件显示勾选且禁用，组件可在 Inspector 编辑、移除，并随场景保存恢复。
  Mesh 显示源模型、三档 LOD GUID、三角形数、页数、子网格和 AABB，可替换模型来源。
  Material 编辑 RGBA、发光、金属度、粗糙度、AO、透明模式/阈值以及七种贴图和通道。
  Scene Lighting 支持主光、点光和方向光的颜色、强度、位置、方向及衰减参数。
  相机显示 is_main，Set Main 调用 camera::set_main()，自动撤销旧相机的主标记。渲染、剔除、LOD 共用唯一主相机。
  Camera 的位姿跟随 Object 的 Transform；删除主相机时回到引擎默认相机。
  相机面板包含四段 LOD 宽度及移动、转向、平移、缩放、加速设置。
- Project：默认只显示用户 src；勾选 Library 查看派生资源。搜索资源，双击模型加入场景，双击场景打开，拖入文件或文件夹导入。
  原始模型不会被修改；依赖会复制到工作空间，派生数据写 Library。
- Console：显示导入、保存及编译消息。
- File / Save Scene 或 Ctrl+S：保存物体、变换、材质覆盖和主相机配置。
- Window / Reset Layout：恢复默认停靠布局。

鼠标位于 Scene 内时，右键 + WASD/QE 飞行，Shift 加速；右键拖动转向，
中键平移，滚轮沿视线移动，F 或 Frame Selected 聚焦选中物体。
窗口只写入 `camera_input`，主相机持有的 `camera_behavior` 在每帧更新中消费输入。
变换角度在 Inspector 中用度显示，保存时仍使用引擎的弧度。

## 全局 Pass 配置

Rendering / Global Pass Settings 展示 `shader/core/pass` 对应的全部阶段，以及
pass → shader → sub_shader → 源文件的 GUID 和路径。前后处理配置可编辑；非透明和
透明阶段在此只读展示。内置 shader 绑定在启动渲染器时从这些配置解析。
GPU 视锥体剔除、Skybox、Post-processing 的 enabled 开关逐帧读取；其余自定义调度尚未实现。
Geometry 独立阶段暂未接入，配置中的 shader_guid 为 null。
默认不透明和透明材质共用 shader/light/pbr.glsl，程序化天空与 PBR 共用 main_light 太阳数据。
Scene Lighting → Procedural Sky 编辑大气、地面颜色和曝光，参数随场景保存。
最终 Post-processing 统一处理 HDR 曝光、ACES 和 sRGB，透明颜色和 Alpha 先在线性空间合成。

`+ Shader` 创建计算着色源、sub_shader 和 shader 配置。Project 选择 shader
可编辑阶段引用；选择 sub_shader 可编辑源文件引用、阶段、入口和宏，
Save & Compile 通过现有扫描器编译并更新 Library。

透明阶段依次执行 capture → compute sort → resolve，按深度合成最近 16 层的预乘
RGBA，再混合到场景颜色；更远片元延续原有溢出混合策略。透明分类按材质槽判断，
Object 的 Force Transparent Stage 可以将整物体送入透明管线，不会清掉材质颜色。

模型导入和内置几何创建均在现有 `src/core/object/object_loader.cpp`：
`object_loader::load_model(path, target)` 导入源文件并创建或替换 Object；
`object_loader::create_geometry(geometry_type)` 创建几何 Object；
`object_loader::describe()` 返回可保存的 Object 配置（含源 GUID、LOD、AABB 和组件）。
返回的 Object 由调用方持有，前端用 unique_ptr 管理。

模型经 Assimp 导入，再由 object_loader 生成运行对象和 `.object` 配置，支持外部
贴图、内嵌贴图、OBJ/MTL、FBX、glTF/GLB 等静态网格。节点变换会烘焙到网格，
目前没有保留骨骼动画、节点层级及原 DCC 的专用材质节点图。纹理从 Library 的
BC1/BC3 缓存上传 GPU，保留 mip；glTF 金属/粗糙度使用 B/G 通道。
LOD 目标为 50%/35%，QEM 保留边界、接缝与拓扑，受约束时实际面数会高于目标。
Scene 网格和选框仍是面板叠加线。

## 验证

```powershell
cmake -S . -B build -DRENDER_BUILD_PRIM_TESTS=ON -DRENDER_BUILD_CULL_GPU_TESTS=ON
cmake --build build -j 4
ctest --test-dir build --output-on-failure
& D:\Project\render_text\build\render_text.exe --smoke --frames 100
```

`--smoke` 使用隐藏窗口，执行相机输入、窗口缩放、网格 GPU 就绪与场景保存回读检查，
并短暂切换透明材质验证合成（随后恢复）。通过 ImGui 鼠标事件实际点击七项添加组件菜单，
验证挂载数量和 Object 配置保存重载，最后打开全局 Pass 面板。至少运行 100 帧。
会写入指定工作空间，请使用测试目录。可同时启用 Vulkan
validation / screenshot layer 检查 GPU 调用及抓取渲染帧。
