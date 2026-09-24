# Metal Replay 验收分工

本规则从 `BATCH29-30.md` 开始执行。目的是把可重复的数据检查交给命令行，
把 qrenderdoc 中真正需要人观察的交互压缩成一次批末验收。

## 由 agent 完成

1. 实现、增量构建、native 运行、capture/XML、Replay API 的 action/state/usage、
   event seek、readback、像素与原始字节断言、异常 RDC 拒绝、逐份 CLI replay、
   lifecycle、脚本检查和 `git diff --check`。L3 仍只在批次清单条件触发时执行。
2. 使用脚本或 Replay API 验证可以自动判定的 HTML/CSV/raw/DDS 内容与保存路径。
   对 qrenderdoc 自身导出按钮的可用性，则留给下面的可见交互验收。
3. 在最终构建上生成批内正式 `.rdc` 和可对照的输出，核对文件存在且与当前代码匹配；
   准备好可直接打开的 qrenderdoc 和 capture 路径。尽量复用一个最终构建、同一组正式
   capture，不让用户承担构建、终端命令、日志分析或重复的数值检查。
4. 在需要 L4 前填写当前批次的“一次性 GUI 验收单”，并在结果中交付：准确路径、
   逐步点击方式、每步预期画面/文字、哪些观察可同时覆盖多个检查项，以及最短反馈格式。
   EID、资源编号、数值和颜色必须来自最终 capture/自动证据，不凭计划猜测。
5. 每个 T 的 GUI 状态写入 [QA_PENDING.md](QA_PENDING.md)；新功能开始时就提示后续
   需要人工 QA，批次交接时保留全部未验项。

## 由用户完成的 L4

- 一次打开最终构建的 qrenderdoc，在同一轮依次检查批内正式 captures。优先用少量
  代表性事件同时核对 Event Browser/API Inspector、关键 Pipeline/Viewer、资源跳转、
  可见输出、保存/导出入口和状态栏；已由自动断言证明的逐像素或原始字节不重复手工核算。
- 用户按验收单回复“全部符合”，或按 capture/EID/步骤写明差异；截图或导出文件在
  有差异时提供即可。agent 不把“验收单已发出”当成 L4 通过，也不把“自动验证通过”
  写成阶段关闭。
- 若结果不符，agent 先分析自动证据与用户反馈，能定位的就修复并重跑受影响检查，
  再给出最短复验步骤。若步骤不清楚，agent 指导用户重新 QA；只有沟通仍无法确认，
  或用户明确要求时，agent 才用 Computer Use 操作 qrenderdoc 定向诊断。
- 如果用户暂时未验、漏看验收单或只反馈部分步骤，未确认的功能持续记为
  “自动验证通过，等待用户 L4”。agent 在 [QA_PENDING.md](QA_PENDING.md) 和
  `STATUS.md` 保留恢复检查点；可继续开发后续功能，但在每次结果和交接提示中
  列出仍待人工 QA 的 T，并优先合并到下次验收单。时间经过或进入下一批不改变状态。
  用户明确确认相应 L4 后，才同步 BATCH、PHASE、STATUS、TEST_MATRIX、README、
  PLAN 和 HANDOFF，并按关闭规则收口。

## 一次性 GUI 验收单应包含

1. **准备**：最终 qrenderdoc 路径、批内正式 `.rdc` 的绝对路径；agent 已完成的
   自动验证摘要和 L3 决策。
2. **步骤与预期**：按 capture 和 EID 排序，写明在 Event Browser 选什么、在
   API Inspector/Pipeline/Texture/Buffer/Resource 看什么，以及输出应显示的颜色或形状。
   将同一 EID 可一次查看的项目合并，优先只选前后两个关键事件。
3. **保存/导出**：只列尚需确认的按钮、建议保存路径和预期文件类型；agent 随后可用
   命令行核对文件内容，不要求用户手工比对字节。
4. **反馈**：给出“全部符合”或“某 capture、EID、步骤与预期不同”的简短回复格式；
   状态栏应为 `No problems detected`。预计时长按真实步骤填写。

本规则不追溯已通过 L4 的 T00–T27；旧批次无需用户重复验收。
