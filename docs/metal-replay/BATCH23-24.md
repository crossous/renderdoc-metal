# 批次 23–24：多命令与 indexed ICB 联合验收

本批固定包含 `PHASE23.md` / T22 多命令 render ICB 和 `PHASE24.md` / T23 indexed ICB。
T22 从 T20 单命令扩展 command index 与非零 execute range；T23 在同一 ICB 路径接入 indexed
command，复用 T14/T21 已验证的 index、base vertex/instance 和 IA/Mesh 语义。批次边界为
P23.1–P24.4，不自动加入第三个 T。实现扩大影响范围时，先更新相关 PHASE 与本联合清单并写明原因。

状态：已关闭。T22/T23 在最终联合 L0/L1/L2、CLI/lifecycle 和同一轮最新 qrenderdoc
L4 通过后一起关闭；下一批为 `BATCH25-26.md`，第一项 `PHASE25.md` P25.1。
保留 BATCH21-22 及此前全部未提交改动。

## 执行顺序

1. T22 按 native fixture → capture/XML → replay/readback/action/state/错误诊断推进；每一步立即跑
   必要自动验证及实际受影响旧场景。自动通过后记录“批末 UI 待验”，继续 T23。
2. T23 同样立即闭环 native/capture/replay，发现共用 ICB 路径改动时重跑 T20/T22 的对应自动项。
3. 最终构建对下面清单去重执行 L0/L1/L2、逐份 CLI replay 与 lifecycle；既有结果只有在最终
   代码未受后续修改影响时才复用。
4. 最新 qrenderdoc 同一轮依次验收 T22/T23；Computer Use 只补自动检查无法证明的可见交互。
5. 全部通过后一起关闭两个阶段并同步 `STATUS.md`、`TEST_MATRIX.md`、`README.md`、`PLAN.md`、
   `HANDOFF.md` 与必要的 `DECISIONS.md`；留下下一批第一项。

批内状态限于“进行中”“自动验证通过，批末 UI 待验”“已关闭”。自动通过不等于阶段关闭。

## 最终联合验证清单

- L0：受影响 target 的最终增量构建、脚本语法及 `git diff --check`。
- L1：T22/T23 各自 native、capture/XML、Replay API action/state/usage/readback/seek、错误输入拒绝、
  CLI replay 与本场景 lifecycle；验证 clear→各 draw→回退、原始 buffer 字节和最终像素。
- L2 必跑一次：T20（原单命令 ICB）、T21（indexed 参数与通用 index 状态）、T01（直接
  draw/output 基线）、T02（indexed IA/Mesh）、T13（非索引 indirect action/usage）、T14
  （index offset/base vertex/base instance）。这是两阶段共用代码路径的去重并集。
- L2 条件触发：若修改通用 per-instance vertex 输入/布局，追加 T05；若修改 vertex resource
  分类、descriptor 或 `VS_Resource`，追加 T16/T19；若修改 render-pass 执行/attachment，追加
  T06/T07/T08；其他路径先补明确 T 编号与原因。
- L4：最新 qrenderdoc 同一轮核对 T22 的非零 execute range、多个展开 draw 的顺序与各自
  IA/Mesh/Buffer/Resource、跳转/保存/HTML/raw export、输出和状态栏；核对 T23 的 indexed
  ICB command、index 类型/offset/base vertex/instance、IA/Mesh/Buffer/Resource、跳转/保存/
  export、输出和状态栏。两份均检查 Event/API 与 `No problems detected`。
- L3 默认不跑。若 L1/L2 暴露无法由以上编号圈定的跨场景风险，或进入发布/合并门槛，记录
  原因后运行最终代码的全部 T00–T23 native、capture/XML、replay/output、逐份 CLI replay
  与 lifecycle；T23 未实现时范围到当时最新 T。具体风险已修复且可由定向场景界定时按联合
  清单收口。

不得为了缩减验收删减必要功能或断言。高级能力是否另立场景由阶段边界和实际证据决定。

## 批次实际结果

- 最终构建见 `/tmp/t23-build.log`，联合命令 `/tmp/run-metal-batch23-24.sh`。
  T22/T23 与必跑 T20/T21/T01/T02/T13/T14 各自 native 5 帧、capture 8 帧、
  XML、Replay API、逐份 CLI replay 均通过。T20/T21/T22/T23 分别有 6/9/11/18 类
  异常 capture 被拒绝；含 T00 的 9 份 capture × 10 轮 lifecycle resident growth
  1769472 bytes。汇总 `/tmp/batch23-24-final.log`；脚本语法检查与
  `git diff --check` 通过。
- 最新 qrenderdoc 在同一进程依次打开 SHA-256 与正式 capture 相同的
  `/private/tmp/t22-batch-ui.rdc` 和 `/private/tmp/t23-batch-ui.rdc`。T22 显示
  `executeCommandsInBuffer(range 1+2)` 与独立 EID 3/4，逐命令 IA/Mesh、Buffer 18/19、
  ICB `Indirect argument` usage、红蓝输出与 HTML/CSV 保存通过。
- T23 显示 `executeCommandsInBuffer(range 1+1)` 与 `ICB[1] drawIndexedPrimitives(3)`；
  EID 3 API 指向 ICB 19。Pipeline IA 显示 Buffer 18 的 UInt16 index、offset 4、
  size 6，以及 Buffer 16/17 的 Vertex / 1、Instance / 1 布局。Mesh VS Input 的
  两个实例、Buffer 18 子范围、Resource Inspector 的 Index Buffer 和 ICB
  `Indirect argument` usage、左红右蓝输出均通过。两份状态栏均为
  `No problems detected`。T23 保存 `/private/tmp/t23-pipeline-final.html`、
  `/private/tmp/t23-index-buffer-final.csv` 和 `/private/tmp/t23-output-final.dds`；
  T22 保存 `/private/tmp/t22-pipeline-final.html`、`/private/tmp/t22-buffer-final.csv`。
- 条件旧场景未触发。定向清单已覆盖本批的 ICB、direct/indirect、indexed 路径，
  未见无法圈定的跨场景风险，也未进入发布/合并门槛；L3 未触发。
