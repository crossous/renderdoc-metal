# 批次 27–28：ICB 继承 pipeline 与 buffers 联合验收

本批固定包含 `PHASE27.md` / T26 `inheritPipelineState` 和 `PHASE28.md` / T27
`inheritBuffers`。T26 验证 render encoder 外层 pipeline 能成为 ICB draw 的真实 draw-time
状态；T27 验证外层 vertex buffer/offset 能被 command 使用并进入 IA/Mesh/usage。边界为
P27.1–P28.4，不自动加入 GPU 生成、compute ICB、heap、blit ICB 管理或多 queue。
影响范围扩大时，先更新两份 PHASE 与本清单的 T 编号和原因。

状态：已关闭。T26/T27 的最终联合自动验证和同轮最新 qrenderdoc L4 均通过；
下一批为 `BATCH29-30.md`，第一项 `PHASE29.md` P29.1。保留此前全部未提交改动。

## 执行顺序

1. T26 按 native fixture → capture/XML → replay/readback/action/state/错误诊断推进，每一环节
   立即跑必要自动验证和受影响旧场景；自动通过后记“批末 UI 待验”，继续 T27。
2. T27 同样立即闭环；若修改 T26 或非继承 ICB 公共路径，重跑受影响的 T26/T20/T22/T24/T25。
3. 最终代码上按下面的并集去重执行 L0/L1/L2、逐份 CLI replay 和 lifecycle。
4. 用最新 qrenderdoc 在同一轮依次验收 T26/T27 正式 capture；Computer Use 只核对自动
   检查不能证明的可见交互。
5. 全部通过后一起关闭两个阶段并同步阶段、矩阵、状态、索引、计划与交接文档。

批内状态限于“待开始”“进行中”“自动验证通过，批末 UI 待验”“已关闭”。

## 最终联合验证清单

- L0：受影响 target 的最终增量构建、脚本语法检查、`git diff --check`。
- L1：T26/T27 各自 native、capture/XML、Replay API action/state/usage/readback/seek、异常
  descriptor/command/resource/range 拒绝、CLI replay 和本场景 lifecycle；核对每个 execute/draw
  的外层状态、原始 buffer 字节、clear→draw→回退与最终像素。
- L2 必跑一次：T20（单命令 ICB）、T22（多命令/range）、T24（reset/reencode）、T25
  （混合 commandTypes）、T01（直接 pipeline/vertex 基线）、T05（多 vertex buffer 与 instance）、
  T16/T19（vertex resource descriptor 与 IA/storage 分类）。
- L2 条件触发：若改动 indexed 路径，追加 T02/T14/T21/T23；若改动 fragment binding，追加
  T03/T04/T12/T17/T18；若改动 render-pass/attachment，追加 T06/T07/T08；其他影响先补
  明确 T 编号和原因。
- L4：最新 qrenderdoc 同一轮核对 T26 的外层 pipeline 与展开 draw 的 Event/API、Pipeline、
  Mesh、Buffer/Resource、输出和 export；核对 T27 的继承 vertex buffers/offset、逐 execute
  IA/Mesh/usage、跳转/保存/export、输出和状态栏。两份均须显示 `No problems detected`。
- L3 默认不跑。若 L1/L2 暴露无法由以上编号圈定的跨场景风险，或进入发布/合并门槛，
  记录原因后在最终代码上运行 T00–T27 全部 native、capture/XML、replay/output、逐份
  CLI replay 与 lifecycle；T27 尚未实现时范围到当时最新 T。

若本机对 inheritance descriptor、encoder 外层状态或两次 execute 的原生 Metal 行为与计划
不同，先保存 native 证据并更新对应 PHASE 与本批边界，再调整 fixture 或实现。

## 批次实际结果

- 最终脚本 `/tmp/run-metal-batch27-28.sh` 完成 demos、qrenderdoc、renderdoccmd 与 smoke/lifecycle
  增量构建；脚本语法、Python 编译和 `git diff --check` 通过。汇总为
  `/tmp/batch27-28-final.log`，运行目录 `/tmp/metal-batch27-28-final.XnLnvh`。
- T26/T27 的 native 5 帧、正式 capture/XML、Replay API action/state/usage/readback/seek、
  原始 buffer 字节、逐份 CLI replay 全部通过。T20/T22/T24/T25/T01/T05/T16/T19
  定向 replay/output 与逐份 CLI replay 通过；T20/T22/T24/T25 的 6/11/8/15 类及
  T26/T27 合计 14 类异常 RDC 被拒绝。
- 含 T00 基线的 11 份 capture × 10 轮 lifecycle 通过，resident growth 507,904 bytes。
  indexed、fragment binding 和 render-pass 条件路径未改；联合清单未暴露无法圈定的跨场景
  风险，也未进入发布/合并门槛，故 T00–T27 L3 未触发。
- 最新 qrenderdoc 同一进程依次打开正式 `t26_capture.rdc`、`t27_capture.rdc`。T26 两次展开
  draw 的 Pipeline State 为 17/18，Buffer 19、Mesh、resource usage 与红蓝输出正确。
  T27 两次展开 draw 的 Buffer 为 16/17，第二次 offset 16；IA、Mesh、Vertex Buffer usage
  与红蓝输出正确。Event/API、资源跳转、HTML/CSV 导出及 DDS 保存通过，两份均显示
  `No problems detected`。导出内容已核对：T26 HTML 为 EID 5/Buffer 19，T27 HTML 为
  EID 5/Buffer 17 offset 16；CSV 与对应顶点数据一致。产物在 `captures/metal-smoke/`：
  `t26-pipeline-final.html`、`t26-buffer-final.csv`、`t26-output-final.dds` 及同名 T27 文件。
- 当前支持的 inheritance 组合限于本批已验证的 Draw ICB descriptor：T26 继承 pipeline、
  command 自带单个 vertex buffer；T27 command 自带 pipeline、继承外层 vertex buffer。
  fragment buffer inheritance、indexed inheritance 与 GPU 生成 ICB 仍需独立 fixture，
  不由本批结果推断支持。
