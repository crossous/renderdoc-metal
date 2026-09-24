# 批次 21–22：indirect draw 与 ICB 联合验收

本批固定包含 `PHASE21.md` / T20 单命令 render ICB 和 `PHASE22.md` / T21 indexed indirect draw。
两条切片共享 draw action、IA/Mesh、Buffer/Resource 与 qrenderdoc 验收路径。批次边界为 P21.1
到 P22.4；不自动加入第三个 T。若实现改变影响范围，先更新相关 PHASE 验证清单与本文件的
联合清单和原因。

状态：已关闭。T20/T21 在最终联合自动清单和同一轮最新 qrenderdoc L4 通过后一起关闭；
下一批为 `BATCH23-24.md`。

## 执行顺序与中间状态

1. 完成 T20 的 native fixture、capture/XML、replay/readback/action/state、错误诊断与自动 smoke。
   增量构建并运行本阶段必要的 L1、受影响旧 T 的 L2。记录 capture、命令、日志和 UI 待验项；
   状态标为“自动验证通过，批末 UI 待验”，继续 T21。
2. 完成 T21 的同一纵向链路，立即运行必要的 L0/L1 和新增受影响路径的 L2。发现 T20 公共路径
   被后续修改时，重跑受影响的 T20 自动断言。批内不得把 native/capture/replay 错误留到批末。
3. 最终代码稳定后，使用同一次最终增量构建执行下方联合清单。相同 T 编号只跑一次；已在最终
   代码上通过且未受后续修改影响的结果可复用，记录构建版本、capture 与日志。两个 T 的 CLI
   replay 和本场景 lifecycle 在此收口。
4. 用最新 qrenderdoc 在同一轮实机验收中依次打开 T20、T21。Computer Use 只检查自动化无法
   证明的可见交互；数值、像素、参数字节和 raw export 由 Replay API/脚本自动断言。
5. 两个阶段全部验收通过后一起标记关闭，更新 `STATUS.md`、`TEST_MATRIX.md`、`README.md`、
   `PLAN.md`、`HANDOFF.md` 和必要的 `DECISIONS.md`，写好下一任务。只因上下文边界确需新对话时
   才建议切换，并给出从最新检查点接手的短提示。

批内状态只能是“进行中”“自动验证通过，批末 UI 待验”或“已关闭”。前两种状态不等同于
Txx 完成；最终 L4 失败时修复后仅重跑实际受影响的 L1/L2/L4。

## 最终联合验证清单

- L0：受影响 target 的最终增量构建、脚本语法检查及 `git diff --check`。
- L1：T20、T21 各自 native、capture/XML、replay/readback/action/state、异常参数拒绝、CLI
  replay 与本场景 lifecycle；明确核对 clear/draw/回退。
- L2 必跑一次：T01（直接 draw/基础 pipeline）、T02（基础 indexed/IA/Mesh）、T05（多 vertex
  buffer/per-instance）、T13（非索引 indirect 参数/action/usage）、T14（indexed offset、
  base vertex/instance）。T21 补齐通用 index buffer 的 `Index` 类别会影响 T02/T14 的
  Resource Inspector，因此由这两项覆盖分类变化；这五项为两阶段已知公共路径的并集，
  复用同一最终构建结果。批末发现通用 Metal Pipeline 将所有 indirect 参数标为
  `Draw Primitives`，且跳转到 Buffer Viewer 时固定用四字段非索引格式；修正后额外复验
  T13 自动状态，并用 qrenderdoc 对 T13/T21 分别确认非索引/索引参数类型和格式。此修改
  只触及 Viewer 显示逻辑，不扩展 replay L3。
- L2 条件触发：改动 vertex resource 分类、descriptor 或 `VS_Resource` 时加 T16/T19；改动
  render-pass 执行/attachment 时加 T06/T07/T08；其他公共路径变动先补明确 T 编号和原因。
- L4：同一轮最新 qrenderdoc 验收中，对 T20 核对 ICB execute range、展开的 draw、IA/Mesh/
  Buffer/Resource、跳转与导出；对 T21 核对 indirect indexed 参数、index 范围、IA/Mesh/
  Buffer/Resource、跳转与导出；同时确认 T13 的 indirect 参数类型仍显示非索引形式；
  两份批内 capture 均核对 Event/API、输出与 `No problems detected`。
  如自动测试已逐字节证明 raw 数据，UI 只需实际走一次保存入口并核对目标文件。
- L3 默认不跑；本批两个阶段并不自行触发 L3。若 L1/L2 失败暴露无法由上述 T 编号圈定的
  跨场景风险，或进入发布/合并门槛，
  记录原因后运行当前全部 T00–T21 的 native、capture/XML、replay/output、逐份 CLI replay
  与 lifecycle；若触发时 T21 尚未实现，则范围到当时最新 T 编号。若触发原因在最终代码仍成立，
  T21 后须在最终代码上完成 T00–T21 L3；若已由修复和定向测试界定，记录依据后按联合清单收口。

需要 UI 才能定位阻塞时，可提前打开 qrenderdoc 做一次定向诊断；它不替代最终构建的批末 L4。
不得为了缩减验收而删减必要功能或断言；确实不必要的功能须说明依据并先更新范围文档。

## 批次实际结果

- 最终 T01/T02/T05/T13/T14/T20/T21 的 native、capture/XML、Replay API、逐份 CLI replay
  均通过；T20 六类及 T21 九类异常 capture 被拒绝。含 T00 的 8 份 capture × 10 轮
  lifecycle resident growth 196608 bytes。汇总 `/tmp/batch21-22-final.log`，未触发 L3。
- 批末发现通用 Metal Pipeline 的 indirect 参数标签/Buffer 格式误标 T21。先更新阶段与批次
  清单，再修复 Viewer。最终 `build-qrenderdoc` 构建见 `/tmp/batch21-22-ui-fix-build.log`；
  T13 Replay API smoke 通过。UI 修改不影响 capture/replay 核心，先前联合自动结果仍适用于
  最终代码。
- 最新 qrenderdoc 在同一进程依次打开 T20、T21，并额外核对 T13。T20 的 execute/draw、
  IA/Mesh/Buffer/Resource、红色三角形与 HTML/CSV；T21 的 indexed action、6-byte index 与
  20-byte 五字段参数、IA/Mesh/Buffer/Resource、红蓝输出与 HTML/CSV 均通过。T13 仍显示
  非索引 `Draw Primitives` 和四字段格式；三份状态栏均为 `No problems detected`。
  UI 产物为 `/private/tmp/t20-batch-buffer-ui.csv`、`/private/tmp/t20-batch-pipeline-final.html`、
  `/private/tmp/t21-batch-arguments-final.csv`、`/private/tmp/t21-batch-pipeline-final.html`。
