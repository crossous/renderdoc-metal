# 批次 25–26：ICB reset 与混合命令联合验收

本批固定包含 `PHASE25.md` / T24 `resetWithRange` 后重编码和 `PHASE26.md` / T25 同一
ICB 中混合非索引与 indexed draw。T24 验证已编码命令的失效和替换；T25 验证
`MTLIndirectCommandTypeDraw | MTLIndirectCommandTypeDrawIndexed` descriptor、逐命令状态和
两种 draw 的资源区分。边界为 P25.1–P26.4，不自动加入第三个 T。实现扩大影响范围时，
先更新两份 PHASE 与本清单的 T 编号和原因。

状态：已关闭。T24/T25 在最终联合 L0/L1/L2、CLI/lifecycle 和同一轮最新 qrenderdoc
L4 通过后一起关闭；下一批为 `BATCH27-28.md`，第一项 `PHASE27.md` P27.1。
保留此前全部未提交改动。

## 执行顺序

1. T24 按 native fixture → capture/XML → replay/readback/action/state/错误诊断推进，每一环节
   立即跑必要的自动验证及受影响旧场景。自动通过后记“批末 UI 待验”，继续 T25。
2. T25 同样立即闭环；若修改 T24 或更早 ICB 公共路径，重跑受影响的 T24/T20/T22/T23。
3. 最终代码上按下面的并集去重执行 L0/L1/L2、逐份 CLI replay 和 lifecycle。
4. 用最新 qrenderdoc 在同一轮依次验收 T24/T25 的正式 capture；Computer Use 只核对自动
   检查不能证明的可见交互。
5. 全部通过后一起关闭两个阶段，同步阶段、矩阵、状态、索引、计划与交接文档，写明下一项。

批内状态限于“进行中”“自动验证通过，批末 UI 待验”“已关闭”。

## 最终联合验证清单

- L0：受影响 target 的最终增量构建、脚本语法检查、`git diff --check`。
- L1：T24/T25 各自 native、capture/XML、Replay API action/state/usage/readback/seek、异常
  reset/descriptor/command/resource 拒绝、CLI replay 和本场景 lifecycle；核对每个 event 的
  clear→draw→回退、原始 buffer 字节和最终像素。
- L2 必跑一次：T20（单命令 ICB）、T22（多命令与非零 range）、T23（indexed ICB）、
  T01（直接 draw 基线）、T02（indexed IA/Mesh）、T13（非索引 indirect）、
  T14（index offset/base vertex/base instance）、T21（indexed indirect 参数和 index 状态）。
- L2 条件触发：若改动 per-instance vertex 输入/布局，追加 T05；若改动 vertex resource
  分类、descriptor 或 `VS_Resource`，追加 T16/T19；若改动 render-pass/attachment，追加
  T06/T07/T08；其他影响先补明确 T 编号和原因。
- L4：最新 qrenderdoc 同一轮核对 T24 reset 前后命令、execute range、替换后的 draw 与
  Event/API、逐命令 IA/Mesh/Buffer/Resource、跳转/保存/export、输出和状态栏；核对 T25
  混合命令的两个 action、各自 index/vertex 输入、IA/Mesh/Buffer/Resource、跳转/保存/export、
  输出和状态栏。两份均须显示 `No problems detected`。
- L3 默认不跑。若 L1/L2 暴露无法由以上编号圈定的跨场景风险，或进入发布/合并门槛，
  记录原因后在最终代码上运行 T00–T25 全部 native、capture/XML、replay/output、逐份
  CLI replay 与 lifecycle；T25 尚未实现时范围到当时最新 T。具体风险被修复且可由定向
  场景界定时按联合清单收口。

不得为缩减测试删减必要功能或断言。若本机对混合 commandTypes 的原生 Metal 行为与计划
不同，先保存 native 证据并更新 `PHASE26.md` 与本批边界，再调整 fixture 或实现。

## 批次实际结果

- 最终脚本 `/tmp/run-metal-batch25-26.sh` 完成 demos、qrenderdoc、renderdoccmd 与 smoke/lifecycle
  构建，脚本语法、Python 编译和 `git diff --check` 通过；汇总为
  `/tmp/batch25-26-final.log`，运行目录 `/tmp/metal-batch25-26-final.6n0AJS`。
- T24/T25 与必跑 T20/T22/T23/T01/T02/T13/T14/T21 的 native 5 帧、正式 capture/XML、
  Replay API/output、逐份 CLI replay 均通过。T20/T22/T23/T24/T21/T25 的
  6/11/18/8/9/15 类异常 capture 全部被明确拒绝。
- 含 T00 基线的 11 份 capture × 10 轮 lifecycle 通过，resident growth 1,212,416 bytes。
  条件旧场景未触发；定向清单已覆盖 reset、非索引/indexed、direct/indirect 与混合 ICB 路径，
  未见无法圈定的跨场景风险，也未进入发布/合并门槛，因此 L3 未触发。
- 最新 qrenderdoc 同一进程依次打开正式 `t24_capture.rdc`、`t25_capture.rdc`。T24 显示
  `range 0+3`、三个展开 draw 与中间 replacement command，旧紫色命令不可见；逐命令
  IA/Mesh/Buffer/Resource、HTML/CSV 和无错误状态栏通过。
- T25 显示 `range 0+2`、`ICB[0] drawPrimitives(3)` 与
  `ICB[1] drawIndexedPrimitives(3)`。非索引 draw 为 Pipeline State 16、Buffer 18
  offset 16/size 88/stride 24 且无 index binding；indexed draw 为 Pipeline State 17、
  Buffer 19/20 的 Vertex/Instance 输入和 Buffer 21 UInt16 offset 4/size 6。Mesh、Index Buffer
  Resource Usage、红/绿/蓝输出、HTML/CSV/DDS 通过；两份 capture 均显示
  `No problems detected`。
