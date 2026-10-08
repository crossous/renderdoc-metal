# PHASE54：对齐 Vulkan/DX12 的 Metal 基础光追回放

后续初态恢复已由 [PHASE55](PHASE55.md) 接续，以下为本阶段的历史边界与结果。

2026-10-05。接续 B468，范围为黑盒捕获/离线回放与普通图形工作可调试性。
用户准则：每项功能先检查本仓库 Vulkan/DX12 是否有对应实现；支持的沿用
RenderDoc 的资源、提交、事件、回退模型；两者不支持的能力不在 Metal 扩展。
Metal 独有能力只有明确必要性和 GPU 验收依据时单独实现。

## 对照与边界

- [光追支持说明](../behind_scenes/raytracing.rst)：Vulkan/DX12 记录并执行光追工作，
  确保其输出可被后续普通图形工作使用；不支持光追内部调试。本阶段不新增
  AS 内部节点/遍历可视化、交点 shader 单步或光追 Pixel History。
- Vulkan `vk_acceleration_structure.cpp` / `vk_initstate.cpp` 与 D3D12
  `d3d12_command_list4_wrap.cpp` / `d3d12_initstate.cpp`：捕获 AS 的资源依赖、
  原始构建输入与提交，回放时重建/恢复，再执行 ray query 或 dispatch。
  Metal 已有受控 build/refit/copy/compact/TLAS/shader-consumer 链，复用该方向。
- Vulkan `Serialise_vkCmdBeginDebugUtilsLabelEXT` / End / Insert 与 D3D12
  `Serialise_BeginEvent` / EndEvent / SetMarker：普通命令列表标记与 GPU 工作进入
  同一事件树。Metal AS 编码器应直接复用公共 PushMarker/PopMarker/SetMarker，
  不增加专用光追树或 viewer。
- Metal intersection function table 没有与 Vulkan/DX12 一一对应的可变对象；
  其作用与交点 shader/函数记录绑定相关。只补齐执行所需的合法空槽与恢复行为，
  不为其添加调试 UI。Apple 接口允许 optional function handle：
  https://developer.apple.com/documentation/metal/mtlintersectionfunctiontable

## 循环执行与验收

1. B469：AS 三个 marker 接口；交点表单槽/range 清空与恢复；保持非空无效 ID
   拒绝。native → capture → GPU readback → 三方向 seek → malformed → 旧帧 →
   生命周期。结果与具体证据见 [B469](BATCH469_RAY_MARKERS_AND_TABLE_CLEAR.md)。
2. B470：AS encoder updateFence/waitForFence，复用已有epoch依赖校验；
   原生 GPU 写几何 → AS build → shader 查询形成可回放依赖链。
   两份新帧、GPU/readback/seek/负例/旧帧/生命周期通过，见
   [B470](BATCH470_RAY_AS_FENCES_AND_BASELINE.md)。
   同时证明帧前已构建AS的原生结果正确、当前离线明确拒绝，为后续优先缺口。
3. 每批固定后端与 bundle hash，串行 GPU；出现错误先修复、重跑受影响项，
   不以增加样例数代替语义验证。

## 尚不能宣称任意应用光追可用

- `supportsRaytracing` / `supportsRaytracingFromRender` 仍 false。当前普通设备查询
  不能因为若干受控样例通过就宣称完整支持。
- GPU 生成 TLAS instance descriptors、间接实例、Private instance buffer 和
  执行点快照/地址重定位尚未闭环；现有路径限定 CPU Shared 数据和构建依赖。
- 曲线、多 geometry、motion、primitive data/transformation buffers、任意帧前
  AS 初态恢复与提交交错的 CPU 函数表变更仍需独立实现/证明。
- 本机 M2 Pro 上的 Metal ray-query API 证据不等于 M4 硬件加速或 UE 真光追帧验收。
- 普通 Metal shader 单步、Pixel History、PostVS 等另属一般图形调试差距，
  不混入本阶段光追黑盒可用结论。

阶段结论：本机受控基础执行链通过；通用应用光追尚不可用。下一阶段优先帧前AS初态
恢复与执行期实例数据，按Vulkan/DX12重建模型推进，并需要具体真实光追工程/捕获
作为最终验收目标；不能单靠打开设备能力查询替代这些工作。
