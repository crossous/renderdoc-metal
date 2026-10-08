# B543 — Typed heap-query 间接调度与逐次参数核验

2026-10-07，PHASE59。实际 UE 两个 Lumen HW query 都使用 GPU 间接调度；本批补齐已有 typed heap-query 对 `dispatchThreadgroups(indirect)` 的拒绝缺口，完成真实 Native→capture→replay 和针对性旧回归。仍未支持实际 UE 的完整 query 参数/资源契约，不开启生产 `supportsRaytracing` / `supportsRaytracingFromRender`。持续任务 active，未提交/推送/重置，保留用户 RDHeaderView 改动、原 UE 工程和安装引擎。

## 仓库对照与实现

先核对 Vulkan `wrappers/vk_draw_funcs.cpp:28/1277` 的 `FetchIndirectData` / `Serialise_vkCmdDispatchIndirect`，以及 DX12 `d3d12_command_list_wrap.cpp:3668` 的 `SaveExecuteIndirectParameters`。两者保存各次执行位置的参数并保留原生间接调用，事件使用 `Indirect` 资源标记。本批复用 Metal 已有 capture per-use snapshot 与 loading GPU readback 比较，没有将间接调度替换成 CPU direct 调度，也没有使用参数 buffer 最终内容代替各次执行值。

- Coverage65 heap-query preflight 现在接受带精确 `(encoder, ordinal)` 证据的间接调用，核验 command、buffer、offset、三维参数、反射线程数、资源存活/退休/范围和工作量上限。已有 1MiB 参数 buffer、12-byte 参数范围、4-byte offset 对齐限制保留；参数不能兼任该 query 的 output、heap、Header 或 contribution 读取资源。
- 实际重放按 ReplayEpoch 重置编码器 ordinal，每次间接调用推进一次。typed closure 在调用前重新核验；loading 阶段仍逐次读取 GPU 原生参数并与捕获记录比较。每个 query 添加 AS/heap/Header/contribution 的只读 usage、output 的可写 usage 和参数的 `Indirect` usage。
- 修正 typed direct/indirect 路径错误落入 generic buffer fallback 导致 heap 被标为 `CS_RWResource` 的行为。强 oracle 现在同时要求读 usage、禁止这些只读资源被标为 writable。
- 只在间接路径允许 `groups.width=0`，仍要求 y/z 为1、线程为一维、总线程≤4096、输出容量足够。现有 sample 契约仍为16-byte root和有界 uint output；实际 UE 的40/48-byte CBV root、二维调度、GPU descriptor/UB producer-consumer 语义没有据此放宽。
- 新 fixture 使用64-byte Private/Tracked placement 参数，offset0/16/52。GPU 在三次 query 前改写同一 buffer 为1/132/0 groups；捕获时保存每次原值。另一个 fixture 在同一编码器连续执行三次132-group query，真实覆盖 ordinal0/1/2。包括 commandBufferWithUnretainedReferences、初态 compact UE同布局48三角形、帧内64几何和空TLAS。HLSL按 lane%4 重复 hit/hit/miss/miss，核验全部528 uint及未写尾部/保护字，而非只核验前四值。

RT 保持黑盒：不查看 AS 内部，不提供 RT shader 单步或 RT Pixel History。

## 最终固定库验收

产物 `build-macos-debug/metal-ray-b543/final/`，入口 `variants.py`、`acceptance.py`（negative/direct/direct-heap/regressions/fresh-compact/old-heap/ordinary-indirect/lifecycle/preflight）、`freeze.py`；汇总 `final-manifest.json`。构建/GPU测试串行共用 IR 测试锁，有限超时/进程组清理，确认无 UnrealEditor/qrenderdoc 并发。

| 范围 | 当前最终库实际结果 |
|---|---|
| 8个新间接 query场景 | Native/capture/API/CLI loops3 PASS；24次间接 query（17非零、7零工作量）；416次事件选择/EID0 |
| 输出 | UE48 3376/3376/0/0；multi64 4398/4398/0/0；indexed 2542/2542/0/0；triangle 2508/2508/0/0；empty 0/0/0/0；528 uint、argument各执行值/padding、只读usage、AS ID/VA改变均核验 |
| 14个新截取的旧直接 query | Native/capture/API/CLI PASS；448次事件选择/EID0 |
| 4个新截取的旧静态 heap query | Native/capture/API/CLI PASS；64次事件选择/EID0 |
| 上述 query 合计 | 26 capture、928次事件选择/EID0 |
| 新间接坏参数，2来源 | 72组、144次API4/CLI1无GPUwait拒绝；含缺失/重复/错误ordinal、owner、buffer、offset、尺寸、线程、容量、资源角色/storage/tracking |
| 新间接合法格式但原生值不符 | 6组、12次API4/CLI1，要求 GPUwait end及execution-point mismatch诊断；另2合法控制通过。没有计入GPU前拒绝 |
| 普通计算间接调度 | 8份新 capture（serial/concurrent、inline/buffer、retained/unretained），Native/API/CLI及56次reset/seek PASS；原1/3/2执行值与最终零参数；176组/352次无GPUwait拒绝，2组/4次原生执行值不符拒绝 |
| 针对性旧回归 | 56检查PASS，含B536格式、旧large buffer view、fresh163初态纹理、B526八query、B512 multi64 TraceRay、旧compact、六能力与官方sample |
| Fresh Shared compact | 4份新capture、18检查PASS |
| 官方 Apple MIT sample | 当前库重新Native/capture/API/CLI；两scene、270事件/EID0、10 size-query PASS |
| 生命周期 | 2 capture×10 controller open/seek/reset/readback/close PASS，resident growth3063808 bytes，保留64MiB上限 |
| 最终严格诊断 | 1798份完成日志无断言、stream seek、chunk overrun、未知资源类型、resource map/record残留；CPU py_compile / git diff --check PASS |

官方已审查来源 `https://docs-assets.developer.apple.com/published/ade36d76f1bb/AcceleratingRayTracingUsingMetal.zip`，ZIP SHA256 `4ee961f8eac0b15232c0ff29066c1e39672e2dd29f855d0c062cdd686accaf94`。本批从已冻结B513下载副本重新构建，构建方法、SDK/DXC/converter、HLSL/DXIL/AIR/metallib/helper/capture哈希保存在各 sample manifest / regressions/official 下。

最终 dylib及bundle SHA256 `2d7439aee9f8d396d3c95d1649a6e2ed8a2b1458938c38c8cf3348371621e9ad`；GUI `3ba30e36278fa0934013114f38ec2b974b2b1765b9286dcc9372764c32019a91`；复用B537 UE provider `aba8b52b92e43ffd7b64e146874db4d1de3b2a7d1ebd66b113dff089f507b1d1`。最终库副本 `final/librenderdoc.dylib`。库/fixture源码及manifest哈希见final-manifest，所有当前库验收完成后冻结。

## 实际 UE 状态及下一批

本批没有启动UE或新截UE帧；复用B542真实 capture（69862532 bytes、SHA `9359c9e20b13249ab786cd16c2b90010aad1d3f291a78c9760fd3e8208e3117c`，捕获库a138d70a），用当前库分别 normal-open、CPU65、pre-submit65 再验。三者仍API4，无GPUwait、初态上传或frameGPU：normal身份guard，CPU65/pre-submit65动态 AS Header/query namespace闭包不完整。是预期拒绝，**没有把UE replay计为通过**。

B542的两个 compact kind2 child6859/6863、主TLAS216-byte配方及两个空TLAS保留；60 AIR审查、两个实际query绑定/dispatch `[2,66,1]` / `[132,1,1]` 的历史证据仍有效，但本批typed16-byte fixture并不等于实际 root2 的40/48-byte CBV表。64heap1491815424 bytes、initialblob1127834757 bytes仍未越过完整预算验证；不称总预算通过。UE完整输出、事件定位、EID0与资源绑定未验证。

下一B544优先补可审计的 CBV-root query契约与必要多维调度，先用真实转换fixture建立参数、AS Header和buffer/texture/sampler依赖闭包，再接实际Lumen PSO与GPU producer-consumer声明。继续原有完整namespace/预算检查，通过后才允许短UE整帧回放。没有用猜测原始GPU地址或全局flag绕过失败。

完整78IR、75RT、308集中回归、全量旧frame-family、Qt/ARC、官方13坏capture本批**未跑**。历史Qt/ForceCrash与系统死机根因仍未证明修复。初次helper编译冲突/negative设置错误及中间502a库运行保存在 `metal-ray-b543/first/`、`second/`、`intermediate-502a/`，不算最终库PASS；usage修正后在2d7439ae完整重跑。两生产能力仍false。
