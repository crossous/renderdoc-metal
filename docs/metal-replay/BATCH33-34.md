# 批次 33–34：compute 间接 dispatch 与 GPU 生成参数

状态：P33.1–P34.4 的功能、最终联合自动验证及 T32/T33 GUI L4 均通过；
用户已确认 compute indirect CS 栏位与实际 threadgroup 数量摘要。
本批已关闭。正式捕获和验收记录见 `QA_BATCH33-34.md`；
`QA_PENDING.md` 当前无待验项。后续 action 名称一致性工作独立见
`ACTION_NAME_ALIGNMENT.md`。

## 功能边界与顺序

1. T32：CPU 写入 shared buffer 中非零 offset 的三项 threadgroup 数量，
   接通 `dispatchThreadgroupsWithIndirectBuffer:indirectBufferOffset:threadsPerThreadgroup:`
   的 capture、XML、replay、事件、参数 buffer usage 与标准 Viewer。
2. T33：同一 command buffer 中先由 compute 写入参数，随后另一 compute
   encoder 间接 dispatch；验证 GPU 写入值与跨 encoder 可见性。compute ICB、
   heap 和多 queue 留给后续独立场景。

## 最终联合验证清单

- L0：增量构建 qrenderdoc、renderdoccmd、两份 demo/smoke；脚本语法、
  Python 编译和 `git diff --check`。
- L1：T32/T33 各自 native、正式 capture/XML、Replay API action/state/
  descriptor/usage/readback/seek、固定参数与输出、异常拒绝、逐份 CLI replay
  和 lifecycle。
- L2 必跑一次：T01/T10/T11/T13/T21/T28/T29/T30/T31/T32；T32 在 T33
  完成后复验，确保 GPU 参数支持没有改变 CPU 参数路径。
- L2 条件：初始内容或所有权变化加 T00/T09/T10；通用事件树或 pass scope
  变化加 T07/T08/T22/T25；render pass/attachment 变化加 T06/T07/T08；
  descriptor/reflection 变化加 T12/T16/T17/T18/T19。新增影响先补编号和原因。
- L4：用户在同一轮最终 qrenderdoc 中检查 T32/T33 的 Event/API、参数
  Buffer、CS Pipeline、Texture 输出、事件前后状态及状态栏。最终 EID 与
  资源编号从正式 capture 取得，写进合并验收单；未确认项持续留在
  `QA_PENDING.md`。新增或改变的 GUI 导出入口才列入本轮检查。
- L3 默认不执行。若 L1/L2 不能充分圈定跨场景风险，或进入发布/合并门槛，
  记录原因后在最终代码上覆盖 T00–T33 全部 native、capture/XML、Replay API、
  逐份 CLI replay 和 lifecycle。

批次只有在联合自动验证、触发的 L3 和 T32/T33 用户 L4 均完成后关闭。
