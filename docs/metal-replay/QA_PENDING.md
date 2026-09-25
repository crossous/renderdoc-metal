# 待人工 QA 清单

此文件是从 `BATCH29-30.md` 起跨批次保留的 L4 待验登记。每次接手、批次交接和
回复结果前都要读取本表；不要只根据最近一条聊天消息判断是否已验收。

## 当前待验

无。用户已确认 T32/T33 的新版 Event Browser 摘要；此前参数、CS、
资源和画面等项目也已确认。T32/T33、PHASE33/34 与 BATCH33-34 已关闭。
后续 action 名称一致性工作见 `ACTION_NAME_ALIGNMENT.md`；尚未实施，
因此当前不产生新的人工 L4 待验项。T31 GUI 导出入口此前由用户明确免除
本轮复验，不记作 GUI 实测。

## BATCH33-34 验收归档

| 批次 / 功能 | 状态 | 验收单与版本 | 用户反馈 |
| --- | --- | --- | --- |
| BATCH33-34 / T32 compute indirect dispatch | 自动与 GUI L4 通过；批次已关闭 | `QA_BATCH33-34.md`；`t32_capture.rdc` SHA-256 `9aa1432c8d9a…`；GUI `f1ebea2ddb86…`；库 `aa21c9a6983d…` | EID 6/12 的参数、CS、资源、画面、状态栏及新版 `<2, 2, 1>` 摘要已确认 |
| BATCH33-34 / T33 GPU 生成间接参数 | 自动与 GUI L4 通过；批次已关闭 | `QA_BATCH33-34.md`；`t33_capture.rdc` SHA-256 `4478197f9994…`；同一 GUI/库 | EID 7/10/12/18 的参数、CS、资源、画面、状态栏及新版 `<2, 2, 1>` 摘要已确认 |

## BATCH31-32 验收归档

| 批次 / 功能 | 状态 | 最终构建与 capture | 验收单 | 用户反馈与备注 |
| --- | --- | --- | --- | --- |
| BATCH31-32 / T30 compute sampler | 自动与 GUI L4 通过；批次已关闭 | `build-macos-debug/bin/qrenderdoc.app`；`captures/metal-smoke/t30_capture.rdc`；capture SHA-256 `8bfe33501fad…`；最新库 `7a494e202d1b…` | `QA_BATCH31-32.md` | 原功能已验；用户进一步确认 EID 4/13 顶层及 `$action()` 筛选符合预期 |
| BATCH31-32 / T31 compute batch binding | 自动与 GUI L4 通过；批次已关闭 | 同一 app；`captures/metal-smoke/t31_capture.rdc`；capture SHA-256 `f5ad1c86660b…`；最新库 `7a494e202d1b…` | `QA_BATCH31-32.md` | EID 7、12–14、18 及后续画面/空槽/筛选/状态栏已确认；GUI 导出入口本轮免复验，自动 DDS/raw 已核对 |

## BATCH29-30 验收归档

| 批次 / 功能 | 状态 | 最终构建与 capture | 验收单 | 用户反馈 | 下一步 |
| --- | --- | --- | --- | --- | --- |
| BATCH29-30 / T28 `dispatchThreads` | 自动与 L4 通过 | `build-macos-debug/bin/qrenderdoc.app`；`captures/metal-smoke/t28_capture.rdc`；capture SHA-256 `b54fb5128aaf…`；GUI `b8259e2471ce…`；库 `81738a1bcbde…` | `QA_BATCH29-30.md` | 用户确认原 L4、10×7 Texture 20 DDS、右侧 Input/Output 缩略图；DDS 与自动参考完全一致 | 无 |
| BATCH29-30 / T29 compute buffer binding | 自动与 L4 通过 | `build-macos-debug/bin/qrenderdoc.app`；`captures/metal-smoke/t29_capture.rdc`；capture SHA-256 `99787263f8b9…`；GUI `b8259e2471ce…`；库 `81738a1bcbde…` | `QA_BATCH29-30.md` | 用户确认 Buffer 20、API Inspector、状态 API EID、画面、导出、状态栏、`$action()` 筛选及右侧缩略图 | 无 |

## 2026-09-24 历史反馈与修复状态

- 用户重存的 `captures/metal-smoke/t28-output-ui.dds` 为 408 字节，SHA-256
  `ada9ab66b7c5…`，与 T28 自动参考 DDS 逐字节一致；T28 DDS 项已验收。
  用户确认 T22 父行可展开为 EID 6/7，点父行正常绘制；T22 新行为复验已通过，
  移出待验表。当前剩 T29 `$action()` 行为及本轮 Texture Viewer 缩略图修复的
  T28/T29 最短 GUI 复验。
- 缩略图根因是 Metal 未支持通用 `ReplayOutput::DrawThumbnail` 所需的 Headless
  output。已修复并在最新库 `81738a1bcbde…` 上通过 T01/T03/T09/T11/T12/
  T16/T17/T22/T25/T28/T29 共 11 份 Replay API 和逐份 CLI replay；
  12 份 capture × 10 轮 lifecycle 通过。自动证据不替代右侧缩略图 GUI 观察。

- 以下为缩略图修复前的历史检查点；顶部表格及本节前两条是当前状态。
  当时用户确认 T25 的多个 instance 显示、ICB 子 draw 收入 execute、点击 execute
  可画到最后一个子 draw；还确认 T27/T28 的所见画面与 `setBuffer` 等 API 有 EID。
  T25 此次新行为最短 L4 已通过，移出待验表；T27 原 L4 已通过。T28 的 UI DDS
  文件未提及，磁盘上仍是旧 Texture 30 文件。T29 的 EID 存在已确认，但
  `$action()` 隐藏/恢复尚未明确反馈。T22 指定 EID 5 的最终红蓝画面未明确反馈。

- 用户已确认 **T29 Buffer 20 变化**和新版 API Inspector 中 `setBuffer` 等调用可见。
  旧表中 T29 的“EID 2/5 待验”已被此反馈覆盖；旧 EID 已失效，不再要求重复。
- Metal 事件树无筛选时缺少状态调用、ICB 父子层级/父行 seek 和 indexed instanced
  子行数量为本轮新缺陷。已在 frame load 中为非 action 调用分配 EID，ICB 父行改为
  MultiAction，点击父行回放至最后子 draw，范围显示 location/length，子行显示
  `instances=2`。正式 T28/T29 captures 已重生成，当前 GUI binary SHA-256
  `b8259e2471ce…`。T29 当前 EID 10/11 为两条 setBuffer，EID 12 为 dispatch。
  Replay API/CLI、异常拒绝与 lifecycle 定向通过；上述新 GUI 行为仍须用户 L4。
- T28 的 10×7 Texture 20 UI DDS 已重存并核对通过。当前 EID 7 是 dispatch，
  缩略图复验步骤见新验收单顶部。
- T22/T25 原功能验收继续有效，仅对本轮改动的 Event Browser 层级、父行画面和
  实例数重新登记最短复验。BATCH29-30 仍未关闭；旧批次不因这次待复验追溯关闭状态。

## 登记与状态规则

1. 新功能开始时就在本表按 T 编号登记“开发中，后续需人工 L4”；同时在本轮结果中
   提醒该功能将需要人工 QA。功能尚未形成正式 capture 时不要求用户提前测试。
2. 最终自动验证完成后，填入最终构建、正式 capture、验收单路径或对应回复，以及
   最后一次影响 GUI 的代码/构建标识；状态改为“自动通过，待人工 QA”。同一批可给
   用户一份合并验收单，但每个 T 的状态分别保留。
3. 只有用户针对明确的验收单回复“全部符合”，或逐项明确通过，才能把对应 T 的
   L4 标为“已通过”。收到部分反馈时只更新已明确覆盖的项；未提到的步骤和 T
   继续“待人工 QA”。未回复、漏看结果、聊天结束、时间经过、下一批开始，均不构成通过。
4. 若反馈不符，记录 T 编号、EID、步骤和现象，状态为“需修复/复验”；修复后 agent
   重跑受影响自动检查，更新 capture/验收单并给最短复验步骤。影响待验功能的后续
   代码或构建变化，也要判断旧验收单是否失效；失效则更新证据和待验版本。已验收
   的功能若被后续 GUI 变更实质影响，也应重新登记受影响的最短 L4 复验项。
5. 批次只有在其所有 T 的 L4 均“已通过”且其他关闭条件也满足时才可标“已关闭”。
   可以继续开发下一功能或下一批，但要在 `STATUS.md`、本表、交接提示和每次结果中
   列出全部仍待人工 QA 的 T；后续验收单优先合并这些未验项，避免重复打开程序。
6. 不自动催促或替用户补做 GUI QA。若用户说不理解或反馈不符，先解释/修复并给
   精简复验；沟通仍无法确认或用户明确要求时，才由 agent 用 Computer Use 定向检查。

本表记录结果而非替代验收单。T28/T29 的正式 captures 与验收步骤见
`QA_BATCH29-30.md`；T30/T31 的正式 captures 与验收记录见 `QA_BATCH31-32.md`。
当前无人工 GUI L4 待验项；T30–T33 所在批次均已关闭。
