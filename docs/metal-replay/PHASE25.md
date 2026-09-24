# 阶段 25 细化计划：T24 ICB resetWithRange 与重编码

T20/T22/T23 已覆盖 CPU 编码的单命令、多命令和 indexed render ICB，但 bridge 的
`resetWithRange:` 仍只转发给真实对象。T24 使指定范围内旧命令的 capture/replay 状态同时
失效，再在同一 command index 重编码。本阶段与 `PHASE26.md` 组成 `BATCH25-26.md`。

状态：已关闭。P25.1–P25.4 自动项及与 T25 同轮的最终 qrenderdoc L4 均通过；下一批
第一项为 `PHASE27.md` P27.1。

## 验证清单

- L1 必跑：T24 native、capture/XML、Replay API action/state/usage/readback/seek、异常
  reset range/空命令拒绝、CLI replay、本场景 lifecycle；每一纵向环节完成后立即验证。
- L2 必跑：T20/T22/T23（已有 ICB command/range/indexed 路径）。
- L2 条件触发：改动普通 draw/action 时加 T01/T13；改动 indexed IA/Mesh 时加
  T02/T14/T21；改动 per-instance 输入时加 T05；改动 vertex resource 分类/descriptor/
  `VS_Resource` 时加 T16/T19；改动 render-pass/attachment 时加 T06/T07/T08。其他影响
  先在本清单与批次清单补 T 编号和原因。
- L4 批末必跑：最新 qrenderdoc 核对 reset 后 execute 仅包含重编码命令、Event/API、
  IA/Mesh/Buffer/Resource、跳转/保存/export、输出和 `No problems detected`。
- L3 默认不跑；定向验证暴露无法圈定的跨场景风险或进入发布/合并门槛时，按
  `BATCH25-26.md` 覆盖当时最新 T，T25 完成后覆盖 T00–T25。

## P25.1：fixture/native

- 新增 `Metal_ICB_Reset_Reencode`，在一个有至少三个 command slot 的 render ICB 中编码
  可区分的旧命令；`resetWithRange` 仅清除中间 slot，再在该 index 编码新命令，执行包含
  前后邻居的 range。固定旧/新颜色与资源哨兵，先跑未注入 native。
- 明确是否需要单命令 `reset` 形式；本阶段只承诺 buffer 的 `resetWithRange`，若 native
  fixture 实际需要其他重载，先更新范围与清单。

验收：输出只有邻居和新命令，旧命令不再影响像素。

## P25.2：capture/replay/event

- 序列化 reset range；replay 对相同范围调用真实 Metal reset，并清空对应 command snapshot。
  后续重编码重建 pipeline/vertex/index 与资源引用，execute 的 marker/draw 仍各占可 seek 事件。
- 拒绝 range 越界/溢出、重置后未编码命令、缺失资源和不支持的 descriptor；核对
  clear→每条 draw→回退与原始字节。

验收：XML、action/state/usage/像素、seek 与错误诊断一致。

## P25.3：标准 Viewer

- 重编码命令的 pipeline、vertex/index 输入与 usage 进入标准 IA/Mesh/Buffer/Resource；
  Event/API 展示最终 command index 与 execute range。自动断言先证明数据，UI 留待批末。

验收：自动状态和资源断言通过。

## P25.4：批内转交

- 完成 T24 必要 L0/L1/L2、CLI/lifecycle，保存 capture、日志和异常诊断，在 `STATUS.md`
  与 `TEST_MATRIX.md` 标记“自动验证通过，批末 UI 待验”，直接进入 P26.1。

验收：T24 自动链路闭环；与 T25 按批次联合清单和同轮 qrenderdoc 一起关闭。

2026-09-24 自动证据：未注入 native、正式 capture/XML、3 个展开 draw 的 Replay API
action/state/usage/readback/seek、旧 command 资源无 VertexBuffer usage、8 类 reset/空命令/资源
异常拒绝、CLI replay、本场景 10 轮 lifecycle 及 T20/T22/T23 定向通过。正式 capture 为
`captures/metal-smoke/t24_capture.rdc`。

2026-09-24 最终 L4：最新 qrenderdoc 与 T25 同一进程打开正式 capture，Event 显示
`executeCommandsInBuffer(range 0+3)` 及 ICB[0]/[1]/[2] 三个 draw；中间 replacement draw 的
Pipeline State 15、Buffer 20 offset 16/size 88/stride 24、Mesh 绿色顶点和 Vertex Buffer usage
均正确，最终画面为红/绿/蓝且旧紫色区域保持 clear。保存
`captures/metal-smoke/t24-pipeline-final.html`、`t24-buffer-final.csv`，状态栏为
`No problems detected`。联合结果见 `BATCH25-26.md`；L3 未触发。
