# 阶段 23 细化计划：T22 多命令 ICB 与非零 execute range

T20 已证明单命令 ICB 纵向链路。T22 将同一 render ICB 扩展为多个独立命令、非零起点的
execute range 和逐命令 action/state；保持 CPU 编码、单 render pass、非索引 draw。
本阶段与 `PHASE24.md` / T23 组成 `BATCH23-24.md`。

状态：已关闭。P23.1–P23.4 自动验证与 `BATCH23-24.md` 的最终联合验收、同轮
qrenderdoc L4 均通过；下一批第一项 `PHASE25.md` P25.1。

## 验证清单

- L1 必跑：T22 native、capture/XML、replay/readback/action/state/usage/seek、异常 range/命令
  拒绝、CLI replay 和本场景 lifecycle；每一纵向环节完成后立即自动验证。
- L2 必跑：T20（单命令回归）、T01（直接 draw/output）、T13（indirect action/usage）。
- L2 条件触发：改动 indexed/IA/Mesh 公共逻辑时追加 T02/T14/T21；改动 per-instance 输入时加
  T05；改动 vertex resource 分类/descriptor/`VS_Resource` 时加 T16/T19；改动 render-pass/
  attachment 时加 T06/T07/T08。其他影响先在本清单及批次清单补 T 编号和原因。
- L4 批末必跑：最新 qrenderdoc 核对非零 execute range、多个展开 draw 的 Event/API、逐命令
  IA/Mesh/Buffer/Resource、跳转/保存/export、输出和 `No problems detected`。
- L3 默认不跑；定向失败暴露无法圈定的跨场景风险或进入发布/合并门槛时，按
  `BATCH23-24.md` 覆盖当时已实现的 T00–T22，T23 完成后覆盖 T00–T23。

## P23.1：fixture/native

- 新增 `Metal_Multi_Command_ICB`，在容量大于执行范围的 ICB 中编码至少三条可区分的非索引
  triangle command，执行从非零 command index 开始的连续两条；未执行命令的资源和颜色用
  哨兵与 CPU 参考图区分。
- 固定各 command 的 pipeline、vertex buffer/offset、绘制参数和输出；先验证未注入 native。

验收：只有所选两条命令影响输出，且顺序可由图像判别。

## P23.2：capture/replay/event

- 保存多条命令的独立编码和 execute range；replay 重建真实 ICB，并对 range 中每条命令建立
  唯一 action/event、资源 usage 和 draw-time state。marker 与各 draw 的事件顺序保持稳定。
- 拒绝越界/溢出 range、未编码命令、缺失资源和不支持的 descriptor/inheritance；验证
  clear→第一 draw→第二 draw→回退、原始资源字节与最终图像。

验收：XML、action/state/usage、逐事件 readback 与错误诊断一致。

## P23.3：标准 Viewer

- 每条展开 draw 的 pipeline、topology、vertex input 与资源范围进入标准 IA/Mesh/Buffer/
  Resource Viewer，Event/API 标明 command index 与 execute range。
- 自动断言先证明数据，UI 可见操作留至批末 L4。

验收：自动状态与资源断言通过；批末 UI 清单明确。

## P23.4：批内转交

- 完成 T22 必要 L0/L1/L2、CLI/lifecycle，保存 capture、日志、异常诊断和 UI 待验项。
- 在 `STATUS.md` 和 `TEST_MATRIX.md` 标记“自动验证通过，批末 UI 待验”，直接进入 P24.1。

验收：T22 自动链路闭环；待 T23 后按批次联合清单与同轮 qrenderdoc 一起关闭。

边界：reset、GPU 命令生成、inherit buffers/pipeline、indexed ICB、compute ICB、heap 与多
queue 由独立场景决定；T23 专门覆盖 indexed ICB。

## 批内自动验证证据

- `Metal_Multi_Command_ICB` 未注入原生 5 帧通过。容量 4 的 ICB 编码 index 0/1/2，
  执行 `1+2`；左红、右蓝、重叠区最终蓝色，未执行绿色区域保持 clear 色。
- capture `captures/metal-smoke/t22_capture.rdc`、XML、Replay API smoke 与 CLI replay 通过。
  execute marker 后是独立的 `ICB[1]`、`ICB[2]` draw 事件；两份 104-byte 顶点包在各自
  draw-time IA/Mesh 显示 offset 16、stride 24，三份原始包及哨兵已核对。clear→第一 draw→
  第二 draw→回退像素通过。
- 11 类异常 range、未编码命令、缺失 pipeline/buffer、越界 offset 与 inheritance capture
  均被 replay 拒绝。T20/T01/T13 定向 smoke、T22 本场景 10 轮 lifecycle 通过。最终联合
  L1/L2/CLI/lifecycle 与 T22/T23 同轮 qrenderdoc L4 见 `BATCH23-24.md`，均通过。
- 最新 qrenderdoc 显示非零 `1+2` execute range、EID 3/4 分别关联 Buffer 18/19、
  Mesh、ICB `Indirect argument` usage、红蓝输出与无错误状态栏。Pipeline HTML 和
  Buffer CSV 已保存并核对；L3 未触发。
