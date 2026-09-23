# 阶段 14 细化计划：T13 单次间接 draw 参数

T12 关闭后，T13 聚焦最小 `drawPrimitives` indirect buffer 路径：CPU 写入一个固定的
`MTLDrawPrimitivesIndirectArguments`，Metal 从 shared buffer 发起一次可辨认 draw。先完成参数 buffer
的 capture/replay/action/Pipeline/Buffer Viewer 闭环，不把 ICB、indirect indexed、GPU 生成参数或
跨 queue 同步混入同一切片。

当前状态：2026-09-23，P14.1-P14.4 已关闭。T13 证据见 `STATUS.md`；后续转
`PHASE15.md` 的 T14 indexed instancing/base vertex。ICB 仍为独立 P2 fixture。

## P14.1：fixture 与 API 盘点

- 新增 T13 最小 fixture，固定 `vertexCount`、`instanceCount`、`vertexStart`、`baseInstance` 和输出像素。
- 盘点 render encoder indirect draw overload、参数结构/对齐、buffer 初始内容、action 与现有
  instancing/Buffer Viewer 状态路径。

验收：native 图像与 CPU 参数参考一致，indirect buffer 字节和 offset 可确定。

## P14.2：capture、replay 与 event

- 只补非索引 `drawPrimitives(... indirectBuffer, indirectBufferOffset)` 的 chunk、序列化和 GPU replay。
- action 使用真实间接参数填充 draw metadata，关联 `Indirect` usage，并验证 event seek、越界与未对齐
  offset 的显式拒绝。

验收：XML、action、usage、前进/回退图像及参数 buffer 数据一致。

## P14.3：标准状态与 Viewer

- Pipeline State 展示实际 topology/vertex binding，indirect 参数通过标准 Buffer Viewer 精确子范围查看；
  Event/API/Resource Inspector 的资源跳转使用同一 ResourceId。
- 验证保存/export 与状态栏，不新建 Metal 专用 indirect 查看器。

验收：自动 smoke 与 qrenderdoc 对同一参数、资源、事件和输出一致。

## P14.4：阶段收口

- 开发中只跑 T13 以及受影响的 T01/T05/T10 buffer/draw 路径；阶段末运行一次 T00-T13 全量回归、
  lifecycle、逐份 CLI replay 和一次最新 qrenderdoc 实机验收。
- 同步阶段文档并决定 ICB 是否进入下一独立 fixture。

验收：完整回归通过，qrenderdoc 状态栏为 `No problems detected`。

## 当前不在本切片内

- indirect indexed draw、GPU/compute 写入参数、ICB、argument-buffer 间接命令、heap、fence/event、
  多 command buffer/queue。

## 关闭证据

- `Metal_Indirect_Draw` 原生通过；16-byte 参数位于 48-byte shared buffer 的 offset 16，四字段为
  `3/2/1/1`，左右实例输出为橙色与蓝色。
- XML 中独立 `drawPrimitives(indirect)` chunk 指向 Buffer 18 / offset 16；replay action 为
  `Drawcall|Indirect|Instanced`，usage 为 `Indirect argument`。clear→draw→clear→draw seek、
  `GetBufferData(16,16)`、精确 `.bin` 与输出像素均由 smoke 验证。
- 对同一 capture 的 XML+ZIP 派生输入分别写入 offset 17（未对齐）和 36（越界）；转换成 RDC 后
  `renderdoccmd replay --loops 1` 均在 indirect chunk 明确返回失败。
- `/tmp/t13-final-regression.log`：T00-T13 全量通过，14 份 capture 各 10 次 lifecycle（resident
  growth 1,294,336 bytes）和逐份 CLI replay 通过。
- 最新 qrenderdoc EID 2 显示 indirect draw；API 与 IA Pipeline 同为 Buffer 18 / offset 16，标准
  Buffer Viewer 精确范围 16/16 显示 `3/2/1/1`。UI `.bin` 与自动 `.bin` 完全一致，Pipeline HTML
  含 Indirect Buffer，Resource Inspector 显示 `Indirect argument`，状态栏为 `No problems detected`。
