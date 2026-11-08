# 程序化天空与默认 PBR

`pbr.glsl` 从原 shader.frag 整理 Cook–Torrance、GGX、Schlick 和 Oren–Nayar，供 opaque 和透明 capture 共用。
顶点入口使用 model 变换世界位置、逆转置变换法线。当前材质采用 metallic/roughness，电介质 F0=0.04（IOR=1.5）。
漫反射只乘一次 N·L，并按 Fresnel/金属度分配能量；AO 只影响间接漫反射。七种贴图和 Alpha 接口保留。
目前没有折射/透射材质参数，Alpha 透明仍通过 OIT 合成。

模型导入不再根据“PNG 含 Alpha”推断透明裁剪。glTF 遵循 alphaMode，其他格式读取显式不透明度、Opacity 贴图和 UseAlpha 标记。
Material → `Base texture alpha is opacity` 控制基础贴图 Alpha 是否参与透明度；打包游戏遮罩时关闭。
需要纹理透明时开启该项，并选择 Mask 或 Blend；基础颜色的 A 和独立 Opacity 贴图仍然有效。
各材质槽的 `Visible` 可关闭辅助几何的绘制，随 Object/Scene 保存，不删除模型数据。
纹理 mip 默认独立过滤 RGBA；只有明确使用 Alpha 透明的颜色图才应启用 texture_config 的 `alpha_weighted_mips`。

`atmosphere.glsl` 实现球形大气的单次 Rayleigh/Mie 散射、臭氧吸收、太阳圆盘和地球遮挡。
世界单位为米，y=0 是海平面，球心 `(0,-earth_radius,0)`。地下观察点按海拔 1m 处理。
视线/太阳光线采用中点积分，默认 16×8 采样；增加采样可改善日落/地平线精度，同时增加 GPU 时间。
尚无多次散射、云、星空、物体投影和镜面环境 IBL；间接光采用大气半球漫反射近似。

`main_light` 保留类名并作为太阳。CPU direction 是光传播方向，乘 Object 旋转后取反写入 GPU；位置和缩放不影响太阳。
最后挂载且未删除的 main_light 生效，删除时恢复上一太阳，全部删除后辐照度为零。
solar_irradiance 是大气层外线性 RGB 法向辐照度，W/m²；默认 20/20/20 为渲染标定值，并非地球太阳总辐照度测量值。
angular_radius 单位 rad，默认 0.004675；disk_intensity 仅调节可见圆盘亮度。
圆盘辐亮度由 `E / (π sin²(radius))` 取得；物体直射光乘同一大气透过率。
旧 main 配置的 color×emission 自动迁移成 solar_irradiance，保存后不再写 position、color、emission、k1、k2。

`skybox_data.hpp` 保存大气/地面参数及单位注释，cpp 打包为 128 字节 std140 UBO。
ground_color 是可见地面漫反射底色；ground_albedo 是间接光近似使用的平均地表反照率。太阳参数不重复保存在 skybox 中。

| Set | 用途 |
| --- | --- |
| 0 | Camera；独立 OIT sort/resolve 管线仍使用局部 set 0 |
| 1 | Light，binding 0 为 32 字节太阳 UBO |
| 2 | Material 与贴图 |
| 3 | 线性 HDR G-buffer |
| 4 | OIT capture |
| 5 | Skybox，binding 0 为大气 UBO |

流程：剔除 → opaque PBR → skybox（远平面，只测深度）→ 透明 capture/sort/resolve → tone_mapping。
光照与透明混合保持线性 HDR；tone_mapping.glsl 最后统一应用曝光、ACES 拟合、sRGB 编码，写独立 UNORM 显示图像。
关闭 post_processing 时跳过曝光/ACES，仍截取到显示范围并编码 sRGB。不在透明片元上单独映射。

Scene Lighting → Procedural Sky 编辑大气/地面；Sun 的 Main Light 组件编辑太阳参数，均随 scene 保存。
pass → shader → sub_shader 定义位于 config/system/render_pass，派生到工作空间 library。
天空/后处理开关逐帧读取，shader 绑定在启动时加载；任意自定义 pass 调度尚未实现。
