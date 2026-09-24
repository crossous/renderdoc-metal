# 阶段 24 细化计划：T23 indexed ICB command

T23 在 T22 多命令 ICB 的编码/执行路径上接入一次 indexed render command，以 T14 的
非零 index offset、base vertex/instance 和 T21 的精确 IA/Mesh 语义作交叉验证。ICB 中编码
直接 index 参数，不使用 T21 的 indirect 参数 buffer。Metal ICB 的 indexed 方法没有独立
`indexStart` 参数，因此本阶段以非零 `indexBufferOffset` 判别索引起点。本阶段是
`BATCH23-24.md` 第二条切片。

状态：已关闭。P24.1–P24.4 与 T22 的最终联合自动验收和同轮 qrenderdoc L4
均通过；下一批第一项 `PHASE25.md` P25.1。

## 验证清单

- L1 必跑：T23 native、capture/XML、replay/readback/action/state/usage/seek、异常 index/
  resource 拒绝、CLI replay 和本场景 lifecycle；每一纵向环节完成后立即自动验证。
- L2 必跑：T22/T20（多命令与单命令 ICB）、T21（indexed 参数与精确 index 范围）、T14
  （base vertex/instance）、T02（基础 indexed IA/Mesh）。与 T22 的公共项批末只跑一次。
- L2 条件触发：修改普通 direct/indirect action 时追加 T01/T13；修改 per-instance 输入时
  追加 T05；修改 vertex resource 分类/descriptor/`VS_Resource` 时追加 T16/T19；修改
  render-pass/attachment 时追加 T06/T07/T08。其他影响先更新本清单及批次清单。
- L4 批末必跑：最新 qrenderdoc 核对 indexed ICB command 的 Event/API、index 类型与非零
  offset、base vertex/instance、IA/Mesh/Buffer/Resource、跳转/保存/export、输出与状态栏。
- L3 默认不跑；定向失败暴露无法圈定的跨场景风险或进入发布/合并门槛时，按
  `BATCH23-24.md` 对最终 T00–T23 执行完整范围。

## P24.1：fixture/native

- 新增 `Metal_Indexed_ICB`，在 CPU 编码的 render ICB 中记录一条 indexed draw。使用非零
  index buffer byte offset、前后哨兵、可判别的 baseVertex/baseInstance 与实例颜色。
- 固定 UInt16 主路径和 CPU 参考数据；是否加 UInt32 由实际共用路径风险决定，决定后先更新
  验证清单。未注入 native 先通过。

验收：GPU 输出与 CPU 参考一致，错误 index/instance 参数能被哨兵区分。

## P24.2：capture/replay/event

- 保存 indexed command 的 index type/buffer/offset、indexCount、instanceCount、
  baseVertex、baseInstance；replay 重建真实 ICB 并将 `Drawcall|Indexed|Indirect`、精确 index
  子范围、resource usage 和 draw-time state 写入标准 action/pipeline 来源。
- 拒绝缺失 index/pipeline/vertex resource、错位或越界 offset、越界 indexCount 与无效
  command range；验证 clear/draw/回退、原始字节与最终像素。

验收：XML、action/usage/IA/Mesh/readback/seek 与错误诊断一致。

## P24.3：标准 Viewer

- IA Pipeline、Mesh VS Input、Buffer/Resource Inspector 和 Event/API 使用同一 index/vertex
  资源与 draw 参数。自动断言精确范围和字节，UI 操作留至批末。

验收：自动数据正确，待批末 qrenderdoc L4。

## P24.4：批次收口

- 在最终代码上执行 `BATCH23-24.md` 去重联合 L0/L1/L2、CLI/lifecycle 与同一轮 T22/T23
  qrenderdoc L4；只在清单条件触发时扩展范围或执行 L3。
- 同步阶段、矩阵、状态、索引和决策文档，记录限制、证据与下一批第一项。

验收：T22/T23 联合自动和最终 UI 均通过后一起关闭。

边界：GPU 生成命令、reset、inherit buffers/pipeline、compute ICB、heap 和多 queue 不在
本阶段支持承诺中；如果实现所需，先更新阶段与批次范围及验证清单。

## 批内自动验证证据

- `Metal_Indexed_ICB` 原生 5 帧红/蓝实例像素通过；8 帧截帧、XML 与 CLI replay 通过。
  `captures/metal-smoke/t23_capture.rdc` 中 command index 1 的 indexed chunk 记录 UInt16、
  4-byte index offset、3 indices、2 instances、baseVertex/baseInstance 1。
- Replay API 逐 draw 验证 `Indexed|Indirect|Instanced`、6-byte index 子范围、两条 vertex
  binding/per-instance layout、ICB/index/vertex usage、原始 12/40/96-byte 资源、
  clear→execute→draw→回退与最终红蓝输出；6-byte raw index 导出为 `000001000200`。
- `util/test/metal/metal_indexed_icb_invalid.py` 的 18 类缺失资源、错位/越界、无效参数与
  command range 均被拒绝。最终 T22/T23 与必跑 T20/T21/T01/T02/T13/T14 联合日志为
  `/tmp/batch23-24-final.log`；9 份 capture × 10 轮 lifecycle 通过。
- 最新 qrenderdoc 同轮 L4 显示 EID 3 indexed ICB draw、UInt16 index Buffer 18
  offset 4/size 6、Buffer 16/17 两种 step layout、Mesh 两实例、Index Buffer 与 ICB
  `Indirect argument` usage、左红右蓝输出及无错误状态栏。HTML、index CSV 和输出 DDS
  已保存，路径与完整联合结果见 `BATCH23-24.md`。L3 未触发。
