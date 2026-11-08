# Morton空间索引、LOD资源和页面管理

## 文件和元数据

每个模型三档LOD分别对应一个 `library/geometry/<source_guid>/<guid>.prim` 文件。资源使用随机生成的GUID，不再包含版本号或把LOD/透明位编码到数字ID中：

1. 第一行：32位十六进制GUID。
2. 第二行：JSON元数据，包含source_guid、内容指纹、LOD层级、透明类别、AABB、顶点/索引字节数和子网格材质槽。
3. 后续二进制：连续vertex数组，接uint32索引数组。

file_scan统一登记这些文件；file_info记录guid、source_guid、三档lod_guids、AABB、lod和transparent。源模型内容未变时只加载几何文件头，复用AABB和目录，不再执行QEM或遍历几何重算AABB。Assimp仍负责读取源模型及材质。缓存缺失时，LOD1/LOD2通过QEM按原模型的50%/35%生成；拓扑和边界约束可能使实际三角形数高于目标。

## 空间索引与页目录

scence_prim负责对象空间索引，保存运行时对象编号、世界AABB及三档资源GUID。128单位一格，直接调用morton::position_to_morton64；跨格对象在所覆盖的格中保留引用。没有Morton坐标范围检查，也没有自定义prim_page_hash或重复的prim_bounds。

SBVH移植桌面项目SLBVH的S层：16桶SAH划分、按面积增量插入、向上更新包围盒、面积增长超过建树时1.5倍时局部重建，以及删除后的节点槽位复用。叶子直接引用对象，暂不接L层。迁移时修正了原上层树的前后缀计数、父节点预占位和重建后的父子索引更新；共面AABB也正确计入表面积。相机大范围查询走BVH，单格查询走Morton索引，避免遍历预加载范围中的空格。

file/config/resource/page_manager是独立CPU页面管理器。虚拟地址为GUID、页号、页内偏移，通过目录映射为文件偏移；不依赖render_resources或Vulkan。内部映射放在cpp，通过互斥锁合并并发请求，并交给thread_pool异步读盘。固定页池默认为64KiB页、256MiB容量、16MiB在途预算；最后一页可不足一页。共享租约使淘汰后的读者仍能继续使用原数据。

## LOD和显存

mesh持有array<mesh_lod_resource,3>、cur_lod、AABB和mesh_data。流式路径中的mesh_data只保留当前子网格目录，顶点/索引存磁盘按需读；没有三档完整CPU副本。mesh.get_bounds返回AABB，由object变换为世界AABB。组件内部操作使用private/protected及友元。

camera只提供相机数据和视锥体，以及可修改的static数组dis。LOD选择全部在mesh，距离取相机位置到世界AABB最近点的欧氏距离：nearest=clamp(camera.position, bounds.min, bounds.max)，distance=length(nearest-camera.position)。最近边缘进入更近的距离段即请求更精细LOD；相机在包围盒内时距离为0。预加载和释放也使用同一距离。四个值是分段宽度：

| 距离 | 行为 |
| --- | --- |
| [0,128) | LOD0 |
| [128,640) | LOD1 |
| [640,1152) | LOD2 |
| [1152,2176) | 预加载LOD2，不绘制 |
| [2176,+∞) | 释放流式几何显存 |

首次进入时先加载LOD2作为粗回退，再加载目标档；目标档全部上传完成前保持已有档位。切换完成后只保留当前档，不常驻LOD2。预加载区可以持有就绪的LOD2；离开加载范围后释放。GPU几何目前按mesh实例持有，相同资源的多个实例尚未共享GPU缓冲。

## 渲染流程

更新对象世界AABB → BVH查询加载范围 → mesh选择/请求LOD → CPU异步页进入staging → 复制到GPU → compute视锥剔除 → 不透明/透明间接绘制 → GPU完成后发布上传状态。

页跨顶点/索引分界时拆成两条复制命令；复制到staging后归还CPU页，GPU完成后释放staging。沿用render_system每帧vkQueueWaitIdle的同步方式。此实现没有多帧在途或屏幕误差LOD。

剔除数据接口在prim_culling_data.hpp，实现及内部参数类型在prim_culling_data.cpp；已去掉结果校验、断言与异常包装。剔除shader在shader/core/pass/pre_processing。候选48字节，参数112字节，间接命令20字节；候选标识为实例下标、LOD、透明标记和保留字段。GUID留在CPU资源索引中。GPU写每条间接命令instanceCount=0/1，render_system按透明标记分pass并绑定子网格材质。几何页保存顶点/三角形索引，不保存光栅化后的屏幕片元。

## 验证

```powershell
cmake -S . -B build -DRENDER_BUILD_PRIM_TESTS=ON -DRENDER_BUILD_CULL_GPU_TESTS=ON
cmake --build build --target Render prim_stream_test geometry_cache_test frustum_cull_gpu_test -j 4
ctest --test-dir build --output-on-failure
```

文件缓存测试覆盖GUID复用、重启恢复、重复扫描、源文件变更和删除。CPU测试还将S层随机插入、移动、删除、重建和槽位复用的查询结果与暴力遍历对照。CPU测试覆盖Morton负坐标、空间移动/删除、并发访问、BVH大范围查询和页面生命周期。Vulkan测试实际dispatch剔除shader并读取GPU顶点/索引缓冲，验证LOD切换、最近边缘跨越四段距离边界、相机在AABB内部、预加载不绘制、材质槽、GUID/AABB文件头和旧LOD显存释放。测试未运行完整项目场景的最终图像。


