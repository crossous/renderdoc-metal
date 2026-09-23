# 阶段 22 细化计划：T21 单次 indexed indirect draw

本阶段是 `BATCH21-22.md` 的第二条切片。T20 ICB 自动链路完成后，接通 Metal
`drawIndexedPrimitives(primitiveType, indexType, indexBuffer, indexBufferOffset,
indirectBuffer, indirectBufferOffset)`，验证 index 参数、非零 offset、base vertex/instance、
事件与标准 IA/Mesh/Buffer/Resource 路径。保持单次 CPU 写入 shared 参数，不扩展到 GPU 生成
参数、ICB indexed command、heap 或多 queue。

当前状态：待 P22.1。T20 自动验证通过并记录 UI 待验后继续；T20 与 T21 在批末一起关闭。

## 本阶段验证清单

- L1 必跑：T21 native、capture/XML、replay/readback/action/state、异常参数、CLI replay 和本场景
  lifecycle；开发中先完成 native/capture/replay 的最小自动闭环，批末按联合清单复验。
- L2 必跑：T13（indirect 参数/action）、T14（indexed offset/base vertex/instance）、T02（基础
  indexed/IA）和 T05（多 vertex buffer/per-instance）。与 T20 公共项在批末只跑一次。
- L2 条件触发：修改 vertex resource 分类、descriptor 或 `VS_Resource` 时加 T16/T19；修改
  render-pass/attachment 公共路径时加 T06/T07/T08；其他影响先在此清单和批次清单补 T 编号与原因。
- L4 必跑但批末执行：最新 qrenderdoc 核对 Event/API、indirect 参数与实际 indexed draw、IA
  index/vertex binding、Mesh、精确 Buffer 范围、Resource Inspector、跳转/保存/export、输出和状态栏。
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
