# 阶段 27 细化计划：T26 ICB 继承 render pipeline

T20–T25 的 render ICB 都由 command 显式设置 pipeline，device replay 也明确拒绝
`inheritPipelineState`。T26 使 descriptor 的 pipeline 继承语义进入 capture/replay，并把
execute 时 render encoder 的真实 pipeline 写入展开 draw 的 action/state。本阶段与
`PHASE28.md` 组成 `BATCH27-28.md`。

状态：已关闭。P27.1–P27.4 自动链路及与 T27 同轮的最终 qrenderdoc L4 均通过；
下一批 `BATCH29-30.md`，第一项 P29.1。

## 验证清单

- L1 必跑：T26 native、capture/XML、Replay API action/state/usage/readback/seek、异常
  descriptor/pipeline/command/range 拒绝、CLI replay、本场景 lifecycle；每一纵向环节后立即验证。
- L2 必跑：T20/T22/T24/T25（非继承 ICB 与逐命令 snapshot）、T01（直接 pipeline/state）。
- L2 条件触发：改动 vertex 输入/资源时加 T02/T05/T16/T19；改动 indexed 路径时加
  T14/T21/T23；改动 fragment binding 时加 T03/T04/T12/T17/T18；改动 render pass 时加
  T06/T07/T08。其他影响先更新本清单和批次清单。
- L4 批末必跑：最新 qrenderdoc 核对 inherited pipeline 的 Event/API、Pipeline、IA/Mesh、
  Buffer/Resource、跳转/保存/export、输出与 `No problems detected`。
- L3 默认不跑；定向验证暴露无法圈定的跨场景风险或进入发布/合并门槛时，按
  `BATCH27-28.md` 覆盖当时最新 T，T27 完成后覆盖 T00–T27。

## P27.1：fixture/native

- 新增 `Metal_ICB_Inherit_Pipeline`，descriptor 使用 `inheritPipelineState = true`，command
  不编码 pipeline；render encoder 在 execute 前设置可判别 pipeline。优先用同一 command 在
  可区分的外层 pipeline 下执行，固定颜色/位置哨兵并先跑未注入 native。
- 若本机要求不同的 descriptor bind-count 组合，先记录原生证据并更新清单。

验收：输出证明 command 使用 execute 时外层 pipeline，而非空状态或先前 command snapshot。

## P27.2：capture/replay/event

- 保存 inheritance descriptor；execute 展开时取得 encoder 当前 pipeline，重建真实 ICB 并保存
  独立 draw-time state。拒绝缺失外层 pipeline、command 自行设置 pipeline、无效 range/资源。
- 验证每个 marker/draw 的 action、usage、seek、像素和原始 buffer 字节。

验收：XML、action/state/usage/readback/seek 与错误诊断一致。

## P27.3：标准 Viewer

- Event/API、Pipeline、IA/Mesh、Buffer/Resource 显示实际继承的 pipeline 与 vertex 输入；
  自动断言先证明数据，UI 留待批末。

验收：自动状态、资源与输出断言通过。

## P27.4：批内转交

- 完成 T26 必要 L0/L1/L2、CLI/lifecycle，保存 capture、日志和异常诊断，在 `STATUS.md`
  与 `TEST_MATRIX.md` 标记“自动验证通过，批末 UI 待验”，直接进入 P28.1。

验收：T26 自动链路闭环；与 T27 按批次联合清单和同轮 qrenderdoc 一起关闭。

## 完成证据

- `Metal_ICB_Inherit_Pipeline` 未注入 native 5 帧、正式 capture/XML、Replay API
  action/state/usage/readback/seek、24-byte vertex 原始数据、CLI replay 均通过。
  同一 ICB command 在两个外层 pipeline 下执行，逐 draw 分别呈现 Pipeline State 17/18、
  左红右蓝；缺失/冲突 pipeline 等异常包含于 T26/T27 合计 14 类拒绝检查。
- 最终联合自动与 11×10 lifecycle 见 `/tmp/batch27-28-final.log`；L3 未触发。
  最新 qrenderdoc 两次 draw 的 Pipeline、IA/Mesh、Buffer 19/Resource usage、Event/API、
  输出与 HTML/CSV/DDS 保存通过，状态栏显示 `No problems detected`。
