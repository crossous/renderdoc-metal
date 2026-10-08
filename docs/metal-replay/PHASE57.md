# PHASE57：官方光追 sample 驱动的功能闭环

2026-10-05，接续 PHASE56；本阶段进行中，公开光追能力仍关闭。
用户授权持续开发、测试修复，sample 达标后再做 UE 实际光追。
持续执行规则和最终出口见 [启用计划](RAYTRACING_ENABLEMENT.md)。

## 当前完成

[B479](BATCH479_RAY_BOX_INITIALS_AND_TABLE_ORDER.md) 已通过两种官方 scene 正式 replay 门槛：
像素与 native 逐字节一致，44/46 EID 各三方向、CLI各3 loops通过。
boxes/compact 依赖与 table-before-pipeline 已补齐，14×10 RT 生命周期通过（growth0bytes）。
固定库全量第三轮308/7,784/3,080通过（growth3,784,704bytes、起止哈希一致）；
旧 scalar0 与 AS residency 负例已改正且保留正例，两轮失败日志保留；
ARC capture 生命周期仍待，随后进入真实 UE 光追。下方失败描述为历史。


[B478](BATCH478_RAY_ARGUMENTS_AND_DISPATCH.md) 已通过官方 triangle 离线像素及 44 事件三轮前后定位；
两场景 capture-probe 均通过。B479 正在补 procedural boxes 初态，能力仍 false。


[B477](BATCH477_RAY_GPU_INPUT_SNAPSHOTS.md) 已打通两种官方场景捕获，输出与原生一致；10 新例 + 12 旧例、
2,262 事件选择与 24 损坏输入通过。GPU 输入快照及 Managed 初态恢复完成，
正式离线 oracle 已接入待验，当前失败推进到 pointer 参数描述；B478 处理中。
下方 B476 的捕获失败为历史。

[B476](BATCH476_APPLE_RAY_SAMPLE_AND_SIZE_QUERIES.md) 接入固定版本 Apple 官方
光追 sample 和可重复原生 oracle，补齐 Managed/Private 实例输入的 AS 尺寸与
heap 布局查询。按照 Vulkan/DX12 查询转发方式解除查询层的 Shared 限制，
保留实际构建、分配和初态恢复的现有校验。

官方 sample 的三角形、程序化球体 scene 原生各两次结果一致。
注入探针已从尺寸查询拒绝推进到 dispatch 缺失有效 TLAS 绑定。
sample 捕获/回放门槛尚未通过，不能称本阶段完成。

## 立即接续

1. 查看 sample 的 capture-triangles.log 和 RenderDoc 日志，复现 Managed
   instance buffer 在 buildRepeatedDistinctInstances 等校验处被拒绝的路径。
   跟踪构建、compact、绑定顺序；所有拒绝必须明确报告，避免传播未构建 AS。
2. 参考 vk_acceleration_structure.cpp 执行点输入复制、d3d12_initstate.cpp
   输入保存/重建，设计 Managed/Private/GPU 输入快照。Managed 不应仅靠替换
   StorageModeShared 检查获得支持；要证明 GPU 可见数据与保存输入一致，
   包括 didModifyRange/synchronizeResource、producer 和提交时序。
3. 将 sample 的 compact TLAS、程序化 boxes 初态、相交函数资源 argument
   buffer 和往返重放闭环；补正式离线输出 oracle，比较实际 sample 结果。
4. 审计 capture ARC 设备与子资源释放顺序断言，增加实际生命周期回归。

按失败证据逐项开发；不盲目全局开启 capability，不删除合法性校验来绕过失败。
扩展 sample 通过后，再使用启用计划中已发现的本机 UE 工程。
