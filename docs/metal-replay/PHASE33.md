# 阶段 33：T32 CPU 参数的 compute indirect dispatch

状态：P33.1–P33.4 功能、自动验证及用户 GUI L4 均通过；阶段已关闭。
用户已确认 CS 间接参数栏与 Event Browser 实际 threadgroup 数量摘要。
本阶段接通 `dispatchThreadgroupsWithIndirectBuffer:indirectBufferOffset:threadsPerThreadgroup:`，
使用 CPU 写入的 shared buffer 参数。与 `PHASE34.md` 组成 `BATCH33-34.md`。

## 验证清单

- L1：T32 native 固定参考、正式 capture/XML、12 字节
  `MTLDispatchThreadgroupsIndirectArguments` 与非零 byte offset、Replay API
  action/state/descriptor/usage/readback/seek、逐事件输出、异常参数拒绝、CLI replay
  和本场景 lifecycle。
- L2 必跑：T11/T28/T29/T30/T31（compute/绑定）、T13/T21（indirect 参数
  与 Buffer Viewer）、T01（最终 render/output）。
- L2 条件：改动通用资源初始内容或所有权，加 T00/T09/T10；改动通用事件树或
  pass scope，加 T07/T08/T22/T25；改动 render pass/attachment，加 T06/T07/T08；
  改动通用 descriptor/reflection，加 T12/T16/T17/T18/T19。发现其他受影响路径，
  先补具体 T 编号和原因。
- L4：批末用户在 Event Browser/API Inspector 核对间接 dispatch、参数 buffer
  与非零 offset，检查 CS Pipeline、Buffer/Texture Viewer、前后事件画面与
  `No problems detected`；导出入口仅在本次新增或改变时列入验收单。
- L3 默认不执行；若 L1/L2 不能圈定跨场景风险，或进入发布/合并门槛，
  在最终代码上覆盖 T00–T33 全部 native、capture/XML、Replay API、逐份 CLI
  replay 与 lifecycle。

## P33.1 fixture/native

建立 `Metal_Compute_Indirect_Dispatch`：在 shared buffer 的非零 4 字节对齐
offset 写入三个 `uint32_t` threadgroup 数量，使用固定 threads-per-threadgroup
和可区分的输出区域；先核对未注入运行的原始参数、输出像素及未触及区域。

## P33.2 capture/replay

补齐 compute encoder 的 ObjC bridge、wrapper/chunk、资源引用及 GPU replay。
XML 应保留 buffer 身份、byte offset 和 threads-per-threadgroup；action 标为
indirect dispatch，并在 seek 前后恢复正确输出。非法 buffer、错位或越界
offset、无效 grid 必须稳定拒绝。

## P33.3 标准 Viewer

自动核对间接参数 buffer 的内容与 usage、CS Pipeline 的绑定、Texture/Buffer
跳转、前后事件输出和 DDS/raw 数据；可见交互留给批末 L4。

## P33.4 批内转交

完成 T32 必要 L0/L1/L2、CLI replay 和本场景 lifecycle，在 `QA_PENDING.md`
登记批末 GUI 待验，接着进入 P34.1。
