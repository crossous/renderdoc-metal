# 阶段 21 细化计划：T20 单命令 indirect command buffer

T19 vertex storage buffer 已关闭。下一条 P2 切片建立一个只含单条 render command 的 Metal
indirect command buffer（ICB），把对象创建、命令编码、执行范围、action/event、资源引用和标准
IA/Mesh/Buffer/Resource Viewer 纵向打通。保持 CPU 编码、单 render pass、单 command，不扩展到
GPU 生成命令、inherit buffers/pipeline、indexed ICB、compute ICB、heap 或多 queue。

本阶段与 `PHASE22.md` / T21 组成 `BATCH21-22.md`。T20 的功能自动链路先验证并记录 UI
待验项，随后推进 T21；两阶段在最终构建上完成联合 L1/L2 和同一轮 qrenderdoc L4 后一起关闭。

当前状态：待从 P21.1 开始；T19 定向 L1/L2/L4 已通过，T00-T18 最近一次完整 L3 仍作为基线，
接手时无需重跑。

## 本阶段验证清单

- L1 必跑：T20 的 native、capture/XML、replay/readback/state、异常参数、CLI replay 和本场景
  lifecycle；开发中先跑功能闭环，批末按联合清单复验。
- L2 必跑：T13（indirect action/usage、参数范围和事件语义）与 T01（基础 render pipeline、直接
  draw 和输出基线）。
- L2 条件触发：若 ICB 实现复用或修改普通 `setVertexBuffer`、IA/Mesh 状态，追加 T02/T05；若修改
  通用 vertex resource 分类、descriptor 或 `VS_Resource`，追加 T16/T19；若修改 render-pass
  执行/attachment 公共路径，追加 T06/T07/T08。出现其他影响时先在此补 T 编号和原因。
- L4 必跑但批末执行：最新 qrenderdoc 打开 T20，核对 Event/API 的 ICB execute 与展开后的
  draw 语义、输出、IA Pipeline、Mesh/Buffer Viewer、Resource Inspector、资源跳转、HTML/raw
  export 和状态栏；与 T21 共用一次实机验收。实现期间必要的 UI 诊断不替代最终 L4。
- L3 默认不执行：若定向失败证明风险跨越上述 T 编号且无法合理圈定，或进入发布/合并门槛，
  则在 `STATUS.md` 记录原因，并按 `BATCH21-22.md` 升级完整范围：T20 期间为 T00–T20，
  T21 完成后为 T00–T21，含 native、capture/XML、replay/output、逐份 CLI replay 与 lifecycle。

## P21.1：fixture 与原生语义

- 新增 `Metal_Indirect_Command_Buffer`：创建固定容量 ICB，在 index 0 编码一条非索引 triangle draw，
  使用确定性 vertex buffer、非零 vertex buffer offset、前后哨兵和可判别输出。
- 固定 descriptor flags、command range、CPU 参考像素和资源可见性声明；先验证未注入 native 输出。

验收：单命令 ICB 在未注入运行时输出与 CPU 参考一致，输入范围可由哨兵判别。

## P21.2：capture/replay/event

- 保存 ICB descriptor、command 编码及 `executeCommandsInBuffer` range；replay 重建真实 ICB，不把
  capture 进程私有对象或不可移植命令字节当作初始内容。
- 为执行和实际 draw 建立明确 action/event/usage，支持 clear/draw/回退，并对越界 command range、
  缺失 pipeline/buffer 和不支持的 inheritance 模式明确失败。

验收：T20 capture/XML、replay output、event seek、资源字节和错误诊断均稳定。

## P21.3：标准 Viewer

- 让 ICB draw 的 pipeline、topology、vertex input 和资源范围进入标准 IA/Mesh/Buffer/Resource
  路径；Event/API 清楚表达 execute range 与实际 draw，不新增 ICB 专用旁路查看器。

验收：自动 state/usage 断言通过；qrenderdoc 对同一 EID 和资源范围的核对在批末完成。

## P21.4：转交批内第二阶段

- 按本阶段清单完成 T20 必要 L0/L1/L2，保存 capture、日志、错误诊断与 UI 待验项；只在列明
  条件触发时扩大定向范围或执行 L3。
- 将 `STATUS.md` 与 `TEST_MATRIX.md` 标为“自动验证通过，批末 UI 待验”，然后直接进入
  `PHASE22.md` P22.1；保留全部既有未提交改动。

验收：T20 的自动功能链路通过且证据可复用；待 T21 完成后按 `BATCH21-22.md` 联合完成
最终自动检查与 qrenderdoc 验收，再将本阶段标为关闭。
