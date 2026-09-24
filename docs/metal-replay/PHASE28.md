# 阶段 28 细化计划：T27 ICB 继承 vertex buffers

T20–T26 的 command 都自行编码 vertex buffer。T27 使用 `inheritBuffers` 让 render encoder
外层 vertex buffer 与动态 offset 成为 ICB draw 输入，验证 execute 时状态、逐事件 snapshot、
资源 usage 和标准 Viewer。本阶段是 `BATCH27-28.md` 的第二条切片。

状态：已关闭。P28.1–P28.4、T26/T27 最终联合自动验收和同轮 qrenderdoc L4 均通过；
下一批 `BATCH29-30.md`，第一项 P29.1。

## 验证清单

- L1 必跑：T27 native、capture/XML、Replay API action/state/usage/readback/seek、异常
  descriptor/buffer/offset/command/range 拒绝、CLI replay、本场景 lifecycle；每一纵向环节后立即验证。
- L2 必跑：T26/T20/T22/T24/T25（继承/非继承 ICB）、T01/T05（vertex/instance 基线）、
  T16/T19（vertex resource descriptor 与 IA/storage 分类）。
- L2 条件触发：改动 indexed 路径时加 T02/T14/T21/T23；改动 fragment binding 时加
  T03/T04/T12/T17/T18；改动 render-pass/attachment 时加 T06/T07/T08。其他影响先更新
  本清单和批次清单。
- L4 批末必跑：最新 qrenderdoc 核对 inherited buffers 的 Event/API、逐 execute IA/Mesh、
  Buffer/Resource usage、跳转/保存/export、输出和状态栏。
- L3 默认不跑；定向验证暴露无法圈定的跨场景风险或进入发布/合并门槛时，按
  `BATCH27-28.md` 对最终 T00–T27 执行完整范围。

## P28.1：fixture/native

- 新增 `Metal_ICB_Inherit_Buffers`，descriptor 使用 `inheritBuffers = true`；command 不编码
  vertex buffer，render encoder 在 execute 前绑定带非零 offset 的可判别 vertex/instance 数据。
  优先用同一 command 在不同外层 binding 下执行两次，先跑未注入 native。
- 原生行为决定 max bind count 与是否同时覆盖 per-instance buffer；扩大范围前先更新清单。

验收：输出能区分每次 execute 的继承 buffer/offset，且无 command 内绑定仍可正确绘制。

## P28.2：capture/replay/event

- 保存 inheritance descriptor；execute 展开时把 encoder 当前 vertex bindings 合入真实 ICB
  执行和 draw snapshot，保留精确 offset/range/step 与资源 usage。
- 拒绝缺失/越界 inherited buffer、command 自行设置冲突 binding、无效 range；验证
  clear→各 execute/draw→回退、原始字节和最终像素。

验收：XML、action/state/usage/readback/seek 与错误诊断一致。

## P28.3：标准 Viewer

- 两次展开 draw 的 IA/Mesh/Buffer/Resource 分别显示 execute 时继承的资源与 offset；
  Event/API 标出 command index/range。自动断言先证明数据，UI 操作留至批末。

验收：逐事件状态、资源和输出断言通过。

## P28.4：批次收口

- 最终代码按 `BATCH27-28.md` 去重执行 L0/L1/L2、逐份 CLI replay、lifecycle 和同一轮
  T26/T27 最新 qrenderdoc L4；只在清单条件触发时扩展范围或运行 L3。
- 同步阶段、矩阵、状态、索引、计划与交接文档，记录限制、证据与下一批第一项。

验收：T26/T27 联合自动及最终 UI 均通过后一起关闭。

## 完成证据

- `Metal_ICB_Inherit_Buffers` 未注入 native 5 帧、正式 capture/XML、Replay API
  action/state/usage/readback/seek、两个 104-byte packet 原始字节、CLI replay 均通过。
  同一 ICB command 两次继承外层 Buffer 16/17，offset 均为 16，逐 draw 红蓝输出正确；
  缺失/越界/冲突 binding 等异常包含于 T26/T27 合计 14 类拒绝检查。
- 最终联合 T26/T27/T20/T22/T24/T25/T01/T05/T16/T19、逐份 CLI replay 与
  11×10 lifecycle 见 `/tmp/batch27-28-final.log`；resident growth 507,904 bytes。
  indexed、fragment binding、render-pass 条件均未触发，L3 未运行。
- 最新 qrenderdoc 同一进程的两次展开 draw、IA/Mesh、Buffer 16/17、第二次 offset 16、
  Vertex Buffer usage、资源跳转、红蓝输出和 HTML/CSV/DDS 保存通过；T27 HTML 为 EID 5
  的 Buffer 17 offset 16，状态栏显示 `No problems detected`。
