# 阶段 29 细化计划：T28 compute dispatchThreads

T11 已覆盖 `dispatchThreadgroups` 的 8×8 texture filter。T28 在同样确定性的 texture
数据流中覆盖 `dispatchThreads` 的总线程维度、非整除 threadgroup 网格、边界保护和
dispatch action/state。本阶段与 `PHASE30.md` 组成 `BATCH29-30.md`。

状态：P29.1–P29.4 自动验证与用户 L4 均通过，阶段已关闭。用户确认
10×7 Texture 20 DDS、右侧 Input/Output 缩略图、画面和状态栏；首次 ⌘O
重复弹窗修复也已验收。证据见 `BATCH29-30.md` 与 `QA_BATCH29-30.md`。

最新：用户已重存 10×7 Texture 20 UI DDS，agent 与自动参考核对完全一致。
Headless 缩略图修复后 T28 Replay API 与右侧 Input/Output GUI 复验均通过。

事件 EID 修复后当前 T28 begin/dispatch/draw 为 EID 3/7/14；用户原验收的画面
与逻辑有效，10×7 Texture 20 DDS 仍需在当前 EID 7 重存。自动验证已在最终
构建与当前正式 capture 上复跑；阶段继续未关闭。

Texture Viewer 缩略图空白缺陷触发 Headless output 修复；追加 T01/T03/T09/T11/
T12/T16/T17/T22/T25/T28/T29 的定向缩略图/原输出验证，当前 T28 的
Input Texture 19、RW Output Texture 20 和 render output Texture 30 需复核。

## 验证清单

- L1 必跑：T28 native、capture/XML、Replay API action/state/usage/readback/seek、
  grid/资源异常拒绝、CLI replay、本场景 lifecycle。
- L2 必跑：T11/T10/T01；修改通用资源范围时追加 T18/T19。
- 事件 EID 语义修复触发 T20/T21/T22/T23/T24/T25/T26/T27 的 indirect/ICB 定向验证；T28
  的 EID、seek、输出和 GUI 步骤以修复后的正式 capture 重核。
- L2 条件触发：改动 shader reflection/descriptor 时加 T12/T16/T17；改动 render pass
  时加 T06/T07/T08；改动初始内容/所有权时加 T00/T09。其他影响先补编号。
- L4 批末由用户按 `QA_GUIDE.md` 的一次性验收单，在最新 qrenderdoc 核对
  Event/API、CS Pipeline、Texture/Resource、跳转/保存/export、逐事件输出和状态栏；
  agent 先完成命令行可判定项并提供最终 capture 路径与准确预期。
- L3 默认不跑；按 `BATCH29-30.md` 的触发条件覆盖当时最新 T，T29 完成后 T00–T29。

## P29.1：fixture/native

- 在 `util/test/demos/metal/` 新增最小 `Metal_Compute_Dispatch_Threads`，使用非整除
  `dispatchThreads` 尺寸和边界哨兵；先跑未注入 native 并核对输入、输出和未触及区域。

## P29.2：capture/replay/event

- 补齐 compute encoder 的 thread-level dispatch capture/chunk/replay、action 维度和 usage，
  验证 XML、逐事件 readback/seek、错误 grid 与资源拒绝。

## P29.3：标准 Viewer

- 自动断言先证明 CS Pipeline、Texture/Resource 与输出数据；UI 留至批末。

## P29.4：批内转交

- 完成 T28 必要 L0/L1/L2、CLI/lifecycle，记录批末 UI 待验，进入 P30.1。

## 2026-09-24 自动证据

T28 的 native/capture/XML/Replay API/异常拒绝与逐份 CLI replay 均通过；最终
联合自动、lifecycle 与 L3 决策见 `BATCH29-30.md`。正式 capture 和用户操作步骤见
`QA_BATCH29-30.md`；用户反馈已收到，L4 通过。
