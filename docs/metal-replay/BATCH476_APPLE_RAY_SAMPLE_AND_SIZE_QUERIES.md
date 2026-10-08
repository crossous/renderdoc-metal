# B476：官方光追 sample 接入与实例 AS 尺寸查询

2026-10-05，PHASE57 首批。支持查询接口和可重复原生 oracle；
sample 捕获仍失败，阶段尚未完成。持续接续见 RAYTRACING_ENABLEMENT.md。

## 跨 API 依据及实现

- DX12 d3d12_device_wrap5.cpp 的 GetRaytracingAccelerationStructurePrebuildInfo
  直接询问设备；Vulkan wrappers/vk_get_funcs.cpp 的
  vkGetAccelerationStructureBuildSizesKHR 解包 AS 后询问设备。
  查询不执行构建，也不需要保存 GPU 工作为 replay chunk。
- Metal 现有 NativeInstanceDescriptor 同时服务查询与实际分配，Shared 输入
  限制导致 Apple sample 的 Managed TLAS 尺寸查询触发 METAL_NOT_HOOKED。
  增加 queryOnly 参数；尺寸和 heap 布局查询对 Managed/Private 输入只解包
  描述符与资源，交给原生设备查询。真实分配仍使用原先校验。
- 不改变 chunk 编号/初态 schema；不放宽 build/refit/copy、初态快照或能力开关。
- 新增 metal_ray_size_query.mm、metal_apple_ray_sample_capture.mm 和
  metal_apple_ray_sample_gate.py。size-queries 可独立执行；sample 下载 ZIP pin、
  解压路径和官方源码一致性检查，保留 Apple LICENSE。源码/产物留在忽略目录。

## 当前精确后端

库和 qrenderdoc bundle 一致：

    309d1058b9db51321d603faa56f58fc47997b55df01a98f7583bfa00acebd864

构建日志：build-macos-debug/ray-samples/apple-basic/build-backend-b476.log。
保留已有修改；未提交/推送。

## 实际测试结果

| 检查 | 结果与范围 |
| --- | --- |
| backend/CLI/app 增量构建 | 通过；Qt SDK 配置警告沿用既有构建环境 |
| 两种三角形、三种 TLAS storage 查询 | 5 组描述符 × AS sizes/heap layout 两接口；原生/注入尺寸逐项一致；描述符和资源引用未被改写 |
| 官方 triangles native | 2 次，各 4 帧；2 geometry/18 instances；finite、nonnegative、非零输出及逐字节重复性通过 |
| 官方 procedural native | 2 次，各 4 帧；3 geometry/27 instances；真实 sphere intersection function；同样通过 |
| 3 个受影响 Shared TLAS 场景重截 | native/capture/API/CLI 均通过，ray 输出正确，34 个 API 事件三方向各例通过，共 306 次选择；每方向 EID 0 reset，CLI 各 3 loops |
| 12 个旧例定向回放 | API/CLI 均通过：t01/t02/t09/t11/t12/t35/t49/t52/t53/t120/t218/t300 |
| 官方 triangles capture-probe | **失败**，详见下方；不能计为捕获通过 |
| 官方 procedural capture/replay、GUI、UE 光追 | **未运行**；因首个 sample 捕获失败，停止后续升级 |
| 全量 308、累计坏捕获、生命周期压力 | **本批未重跑**；此前 B475 证据属于另一 hash，不能移计至本批 |

重截三例为 background-tlas-repeated、background-tlas-indexed-repeated、
background-tlas-indexed-repeated-child-compact；重新编译 helper 后测试当前库。
其日志/manifest 在 build-macos-debug/metal-ray-b476，捕获在 captures/metal-ray-b476。
旧例日志为 build-macos-debug/metal-targeted.slw2RR，
汇总 build-macos-debug/metal-ray-b476/targeted.log。

官方 sample 输出为 64×64 RGBA32Float，各 65,536 bytes，seed=1：

- triangles：e64da5d75d7a53f5c93843a58d796f55dbedcbecc97f063e43eb3cb93ce0bebb，
  RGB 总能量 1580.47478。
- procedural：1a2c5f07731f303b9c4cc7607be3654ae716214b9bcf4e1a722019b7464ca3c4，
  RGB 总能量 1639.59669。

同机重复性检查是建立 oracle；未将其解释为不同 GPU/驱动上的字节一致要求。
正式 replay oracle 接入后，应明确跨设备容差及各资源/EID 预期。

## 原生与捕获差异、尚未解决的问题

1. B475 注入 sample 在 accelerationStructureSizesWithDescriptor 处拒绝；
   B476 的原生/注入查询门槛已通过，Managed/Private TLAS 两接口一致。
2. B476 sample capture-probe 到 raytracingKernel dispatch 时 Metal validation
   报 missing Instance Acceleration Structure binding at index 4；退出非零。
   Managed instance build/compact/绑定链仍有 Shared 限制，须按执行点输入
   复制模型补齐并确认首个被拒绝的构建调用，不直接放宽所有 storage 检查。
   未产生通过验收的官方 sample capture，不进入 replay/UE。
3. 首版 ARC 查询探针在资源描述符尚未 autorelease 时释放设备，触发 capture
   ResourceManager 析构断言。保留 size-query-backtrace.log；当前查询测试改为
   调用者持有设备直至内部 pool 排空，专门隔离查询验证。设备/子资源生命周期
   问题没有在后端修复，仍须审计和回归。

官方 sample 工作目录：
build-macos-debug/ray-samples/apple-basic。
gate-results/size-queries-manifest.json 是绿色查询门槛；
native-manifest.json 是原生门槛；
capture-probe-manifest.json 记录红色捕获门槛、查询通过以及未运行 replay/UE。
最早失败的 B475 capture-gate.log 和 B476 capture-gate-b476.log 分别保留。

本机 UE_5.8、Testproj.uproject 与捕获插件路径已确认，尚未启动 UE 光追。
supportsRaytracing/supportsRaytracingFromRender 继续 false。
