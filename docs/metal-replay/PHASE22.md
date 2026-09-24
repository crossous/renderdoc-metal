# 阶段 22 细化计划：T21 单次 indexed indirect draw

本阶段是 `BATCH21-22.md` 的第二条切片。T20 ICB 自动链路完成后，接通 Metal
`drawIndexedPrimitives(primitiveType, indexType, indexBuffer, indexBufferOffset,
indirectBuffer, indirectBufferOffset)`，验证 index 参数、非零 offset、base vertex/instance、
事件与标准 IA/Mesh/Buffer/Resource 路径。保持单次 CPU 写入 shared 参数，不扩展到 GPU 生成
参数、ICB indexed command、heap 或多 queue。

当前状态：P22.1–P22.4 已关闭；T20/T21 联合自动验证和同一轮最新 qrenderdoc L4 均通过。

## 本阶段验证清单

- L1 必跑：T21 native、capture/XML、replay/readback/action/state、异常参数、CLI replay 和本场景
  lifecycle；开发中先完成 native/capture/replay 的最小自动闭环，批末按联合清单复验。
- L2 必跑：T13（indirect 参数/action）、T14（indexed offset/base vertex/instance）、T02（基础
  indexed/IA）和 T05（多 vertex buffer/per-instance）。T21 为标准 Resource Inspector 补齐
  通用 index buffer 的 `Index` 创建类别，故 T02/T14 同时覆盖该公共分类变化。与 T20 公共项在
  批末只跑一次。批末 L4 发现通用 Metal Pipeline 的 Indirect Buffer 参数类型固定写成
  `Draw Primitives`，且 Buffer Viewer 跳转套用了四字段非索引格式，会误读 T21 五字段参数；
  修正后对 T13 和 T21 同时核对 UI 参数类型与 Buffer 格式，并复验 T13 自动状态，范围限于
  该 Viewer 的显示逻辑。
- L2 条件触发：修改 vertex resource 分类、descriptor 或 `VS_Resource` 时加 T16/T19；修改
  render-pass/attachment 公共路径时加 T06/T07/T08；其他影响先在此清单和批次清单补 T 编号与原因。
- L4 必跑但批末执行：最新 qrenderdoc 核对 Event/API、indirect 参数与实际 indexed draw、IA
  index/vertex binding、Mesh、精确 Buffer 范围及 `Draw Indexed Primitives` 参数类型、
  Resource Inspector、跳转/保存/export、输出和状态栏；修正通用参数标签与格式后另核对
  T13 仍显示 `Draw Primitives` 且为四字段格式。
- L3 默认不执行；若定向失败暴露无法圈定的跨场景风险或进入发布/合并门槛，按
  `BATCH21-22.md` 执行完整 T00–T21 范围并记录原因。

## P22.1：fixture/native

- 新增 `Metal_Indexed_Indirect_Draw`，CPU 在 shared 参数 buffer 的非零 offset 写入
  `MTLDrawIndexedPrimitivesIndirectArguments` 五字段；index buffer 使用非零 byte offset 和
  前后哨兵，固定可区分 indexStart、baseVertex、baseInstance 的图像/数据参考。
- 核对 Metal 参数布局、对齐、index 类型和未注入 readback。

验收：原生像素与 CPU 参考一致，错误 offset/字段可由输入和哨兵判别。

## P22.2：capture/replay/event

- 保存 indirect indexed overload 的 index buffer、参数 buffer 与两个 offset；replay 读取真实
  五字段，并把 `Indexed|Indirect`、实例、base vertex/instance、精确 index 子范围与
  `Indirect`/`IndexBuffer` usage 写入同一 action/state 来源。
- 验证 clear/draw/回退、原始参数/index 字节，以及缺失资源、错位或越界 offset 的明确失败。

验收：T21 XML、action、usage、readback、seek 和异常诊断均一致。

## P22.3：标准 Viewer

- IA Pipeline 展示精确 index/indirect 参数范围；标准 Mesh/Buffer/Resource Viewer 与
  Event/API 使用同一 ResourceId、offset 和 draw 参数。
- 自动 smoke 先断言状态与字节，UI 操作记入批末清单。

验收：自动数据正确，待 `BATCH21-22.md` 的最终 L4 完成实机验收。

## P22.4：批次收口

- 按 `BATCH21-22.md` 在最终代码上执行联合 L0/L1/L2、CLI/lifecycle 和同一轮 T20/T21 L4；
  只有清单所列条件触发时才扩大定向范围或执行 T00–T21 L3。
- 同步两个阶段与索引文档，记录限制、证据及下一批第一项。

验收：T20/T21 都通过最终自动检查和最新 qrenderdoc 验收后一起关闭；之前的“UI 待验”状态
不得提前写为完成。

## P22.1–P22.3 自动验证证据

- `Metal_Indexed_Indirect_Draw` 未注入原生 5 帧通过；参数包 offset 16 的五字段为
  `3/2/1/1/1`，UInt16 index buffer 从 byte offset 4 开始、`indexStart=1`，左右实例分别为红/蓝。
- capture `captures/metal-smoke/t21_capture.rdc`、XML、CLI replay 通过；Replay API 验证
  `Indexed|Indirect|Instanced` action、精确 6-byte index/20-byte 参数范围、原始字节和哨兵、
  `IndexBuffer`/`Indirect` usage、IA/Mesh 与 clear→draw→clear→draw 回退。
- XML+ZIP 派生的缺失 index/参数资源、两个错位 offset、两个越界 offset、越界 indexStart、
  越界/零 indexCount 共 9 类均被 replay 拒绝。T21 单场景 10 轮 lifecycle 通过
  （resident growth 1,032,192 bytes）；日志 `/tmp/t21-*.log`。
- T21 改动通用 index buffer 类别后，T02/T14 与 T20 的 Replay API smoke 已复验通过。

## 最终联合与 UI 验收证据

- 最终代码上 T01/T02/T05/T13/T14/T20/T21 的 native、capture/XML、Replay API 与各自 CLI
  replay 均通过；T20 六类、T21 九类异常 capture 被拒绝；含 T00 的 8 份 capture × 10 轮
  lifecycle resident growth 196608 bytes。汇总 `/tmp/batch21-22-final.log`；L3 未触发。
- 批末 UI 发现 Metal Pipeline 的 Indirect Buffer 一律写为 `Draw Primitives`，且 Buffer Viewer
  跳转固定套用四字段格式。已按本阶段和批次清单修正；`build-qrenderdoc` 重新构建通过，T13
  Replay API smoke 复验通过。新构建的 T21 EID 2 显示 `3 indices, 2 instances,
  indexStart 1, baseVertex 1, baseInstance 1`，API Inspector 显示 UInt16 index Buffer 18
  offset 4、参数 Buffer 19 offset 16。IA index 子范围为 6/6/UInt16、参数子范围为 16/20，
  标签 `Draw Indexed Primitives`；Buffer Viewer 五字段为 `3/2/1/1/1`，index 子范围为 `0/1/2`。
  Mesh VS Input 三行、Buffer 18/19 到 Resource Inspector 的跳转与 `Index Buffer`/`Indirect
  argument` usage 均通过。Texture Viewer 为左红右蓝，状态栏为 `No problems detected`。
  `/private/tmp/t21-batch-arguments-final.csv` 和 `/private/tmp/t21-batch-pipeline-final.html` 已保存
  并核对内容。相同最新 qrenderdoc 中，T13 仍显示 `Draw Primitives` 和 16-byte 四字段格式。
