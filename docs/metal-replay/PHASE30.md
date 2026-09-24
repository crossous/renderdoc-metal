# 阶段 30 细化计划：T29 compute buffer binding

T11/T28 的 compute 数据流以 texture 为主。T29 增加带非零 offset 的 compute buffer
输入与输出，并验证原始范围、CS descriptor/usage 和标准 Buffer Viewer。本阶段是
`BATCH29-30.md` 第二条切片。

状态：P30.1–P30.4 自动验证与用户 L4 均通过，阶段已关闭。用户确认
API Inspector 绑定调用、Buffer 20 哨兵与写入、Event Browser 状态调用 EID、
`$action()` 筛选、右侧 Input/Output 缩略图、画面和状态栏；HTML/CSV/336-byte
raw 导出经 agent 核对。见 `BATCH29-30.md`、`QA_BATCH29-30.md`。

最新：Headless 缩略图修复后，T29 计算前黑色及计算后 Input/Output 有内容的
Replay API 断言通过；右侧缩略图和 `$action()` 筛选的 GUI 复验也已通过。

用户随后确认 Buffer 20 变化和 API Inspector 绑定调用可见。事件 EID 修复后
T29 两条 setBuffer 为 EID 10/11，dispatch 为 EID 12；当前只待 Event Browser
无筛选与 `$action()` 行为的最短 GUI 复验，阶段继续未关闭。

Texture Viewer Headless thumbnail 修复后，追加 T01/T03/T09/T11/T12/T16/T17/
T22/T25/T28/T29 定向；当前 T29 的 Texture 21 input、Texture 22 RW output
与 render output 需复核，GUI 缩略图交互待最短 L4。

## 验证清单

- L1 必跑：T29 native、capture/XML、Replay API action/state/usage/readback/seek、
  buffer slot/offset/range/resource 异常拒绝、CLI replay、本场景 lifecycle。
- L2 必跑：T28/T11/T10/T01/T18/T19。
- 事件 EID 语义修复触发 T20/T21/T22/T23/T24/T25/T26/T27 的 indirect/ICB 定向验证；T29
  的 EID、绑定、Buffer Viewer 和 GUI 步骤以修复后的正式 capture 重核。
- L2 条件触发：改动 reflection/通用 descriptor 时加 T12/T16/T17；改动 render pass
  时加 T06/T07/T08；改动初始内容/所有权时加 T00/T09。其他影响先补编号。
  本次通用 descriptor access 映射已改，T12/T16/T17 条件触发。
- L4 批末由用户按 `QA_GUIDE.md` 的一次性验收单，在最新 qrenderdoc 核对
  Event/API、CS Pipeline、Buffer/Resource、逐事件跳转/保存/export、输出和状态栏；
  agent 先完成命令行可判定项并提供最终 capture 路径与准确预期。
- L3 默认不跑；按 `BATCH29-30.md` 的触发条件覆盖 T00–T29。

## P30.1：fixture/native

- 新增最小 `Metal_Compute_Buffer_Binding`，使用非零 slot/offset 与可区分的输入、输出
  哨兵；先跑未注入 native 并验证前后原始字节。

## P30.2：capture/replay/event

- 补齐 compute buffer binding 的 capture、重放与 descriptor/state/usage，保留精确
  offset/range，验证逐事件 readback/seek 和异常拒绝。

## P30.3：标准 Viewer

- 自动核对 CS Buffer/Resource 与格式化/原始字节；UI 留至批末。

## P30.4：批次收口

- 最终代码按 `BATCH29-30.md` 去重执行联合自动、逐份 CLI replay、lifecycle；
  交付合并 T28/T29 的用户 GUI 验收单，收到同轮最新 qrenderdoc L4 反馈后，
  同步状态、矩阵、索引、计划与交接文档。

## 2026-09-24 自动证据

T29 的 native/capture/XML/Replay API/异常拒绝与逐份 CLI replay 均通过；最终
联合自动、lifecycle 与 L3 决策见 `BATCH29-30.md`。正式 capture 和用户操作步骤见
`QA_BATCH29-30.md`；用户反馈已收到，L4 通过。
