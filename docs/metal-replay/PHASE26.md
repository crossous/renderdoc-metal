# 阶段 26 细化计划：T25 混合非索引与 indexed ICB command

T25 在同一 CPU 编码 render ICB 中交替记录非索引和 indexed draw，验证组合
`MTLIndirectCommandTypeDraw | MTLIndirectCommandTypeDrawIndexed` descriptor 与逐命令
action/state/usage。本阶段是 `BATCH25-26.md` 的第二条切片。

状态：已关闭。P26.1–P26.4、T24/T25 最终联合自动验收和同轮 qrenderdoc L4 均通过；
下一批第一项为 `PHASE27.md` P27.1。

## 验证清单

- L1 必跑：T25 native、capture/XML、Replay API action/state/usage/readback/seek、异常
  command type/index/resource 拒绝、CLI replay、本场景 lifecycle；每一纵向环节后立即验证。
- L2 必跑：T24/T20/T22/T23（reset 与两类 ICB）、T21/T14/T02（indexed 语义）、
  T13/T01（非索引语义）；与 T24 的公共项批末只跑一次。
- L2 条件触发：改动 per-instance 输入时加 T05；改动 vertex resource 分类/descriptor/
  `VS_Resource` 时加 T16/T19；改动 render-pass/attachment 时加 T06/T07/T08。其他影响
  先更新本清单和批次清单。
- L4 批末必跑：最新 qrenderdoc 核对同一 ICB 的两类展开 draw、Event/API、各自
  IA/Mesh/Buffer/Resource、跳转/保存/export、输出和状态栏。
- L3 默认不跑；定向验证暴露无法圈定的跨场景风险或进入发布/合并门槛时，按
  `BATCH25-26.md` 对最终 T00–T25 执行完整范围。

## P26.1：fixture/native

- 新增 `Metal_Mixed_ICB`，在一个 descriptor 和一个 render pass 中编码至少一条非索引
  command 与一条 UInt16 indexed command，使用可区分的 vertex/index buffer 与颜色，执行
  覆盖两条命令的 range。先验证原生 Metal 支持该 descriptor 与预期输出。
- 保留非零 index buffer offset、base vertex/instance 的哨兵。是否追加 UInt32 或更多
  command 由原生证据和共用路径风险决定；若增加，先更新验证清单。

验收：原生输出可区分两条 draw、执行顺序及索引参数。

## P26.2：capture/replay/event

- 允许经 native 验证的组合 descriptor；序列化每条命令的类型与参数。replay 用真实 ICB
  分别执行单命令子范围，保存独立 action/event、draw-time IA 与精确资源 usage。
- 对错误 command type、未编码 command、缺失资源、index 越界及无效 range 明确拒绝；
  验证 clear→第一 draw→第二 draw→回退、原始字节和最终像素。

验收：XML、action/usage/state/readback/seek 和错误诊断一致。

## P26.3：标准 Viewer

- 两类展开 draw 在标准 Event/API、IA/Mesh/Buffer/Resource 中各有正确类型、range 与绑定；
  非索引命令不得继承 indexed index binding，indexed 命令须显示精确 index 子范围。
  自动断言先证明数据，UI 操作留至批末。

验收：两类状态和资源断言通过。

## P26.4：批次收口

- 最终代码按 `BATCH25-26.md` 去重执行 L0/L1/L2、逐份 CLI replay、lifecycle 和同一轮
  T24/T25 最新 qrenderdoc L4；只在清单条件触发时扩展范围或运行 L3。
- 同步阶段、矩阵、状态、索引、计划与交接文档，记录限制、证据与下一批第一项。

验收：T24/T25 联合自动及最终 UI 均通过后一起关闭。

## 完成证据

- `Metal_Mixed_ICB` 未注入 native 5 帧、正式 capture/XML、Replay API action/state/usage/
  readback/seek、逐资源原始字节、CLI replay 和本场景 lifecycle 均通过。descriptor 的
  `commandTypes` 为 Draw|DrawIndexed；非索引 command 使用 Buffer 18 offset 16，indexed
  command 使用 Buffer 19/20、UInt16 Buffer 21 offset 4、baseVertex/baseInstance 1 和两实例。
- `metal_mixed_icb_invalid.py` 的 15 类 descriptor、command type、range、resource、index
  异常均被拒绝。最终联合 T24/T25/T20/T22/T23/T01/T02/T13/T14/T21 自动检查、逐份
  CLI replay 与 11×10 lifecycle 通过，汇总 `/tmp/batch25-26-final.log`；L3 未触发。
- 最新 qrenderdoc 与 T24 同一进程显示两个独立 action。非索引 IA 无 index binding，Mesh 为
  红色三角形；indexed IA 为 Buffer 21 UInt16 offset 4/size 6，Buffer 19/20 分别为 Vertex / 1
  与 Instance / 1，Mesh 的 indices 0/1/2 和实例输入正确，最终输出为左红、右绿/蓝。
  Index Buffer Resource Inspector 显示 EID 4 `Index Buffer` usage；保存
  `captures/metal-smoke/t25-pipeline-final.html`、`t25-indexed-pipeline-final.html`、
  `t25-index-buffer-final.csv`、`t25-output-final.dds`，状态栏为 `No problems detected`。
