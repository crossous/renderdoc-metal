# Metal Replay 当前状态

最后更新：2026-09-24（Asia/Shanghai）

## 2026-09-24 BATCH29-30 关闭与下一批准备

- 用户在同一轮 qrenderdoc 确认 T28 右侧 Input/Output 缩略图正常，以及 T29
  `$action()` 筛选和右侧缩略图正常。此前 T28/T29 Event/API、Pipeline、
  Buffer 20、画面、导出、状态栏均已确认；T28 10×7 UI DDS 与自动参考完全一致。
  T22/T25 因事件树改动所需的最短复验也已通过。PHASE29、PHASE30 与
  BATCH29-30 现已关闭；`QA_PENDING.md` 当前无待验项。
- 自动证据：`/tmp/batch29-30-final.log` 覆盖 T28/T29 native/capture/XML/
  Replay API、10 类异常拒绝、联合 T11/T10/T01/T18/T19/T12/T16/T17、
  逐份 CLI replay 与 11×10 lifecycle。缩略图修复后追加 T01/T03/T09/T11/
  T12/T16/T17/T22/T25/T28/T29 Replay API 与逐份 CLI replay、12×10
  lifecycle，resident growth 1,376,256 bytes。最终 GUI binary SHA-256
  `b8259e2471ce…`、内嵌 replay 库 `81738a1bcbde…`；正式 T28/T29 capture
  SHA-256 分别为 `b54fb5128aaf…`、`99787263f8b9…`。
- 用户明确本次向 `crossous/renderdoc-metal` 的提交仅是备份，不是发布；
  因此本次不运行 T00–T29 L3。上述定向验证与 L4 作为提交依据。
  下一批 `BATCH31-32.md` 计划 T30 compute sampler 直接绑定与 T31 compute
  texture/sampler/buffer 批量绑定；第一项 P31.1，尚未开始实现。

## 2026-09-24 历史检查点：用户验收与下一批准备

- 用户重新保存的 `captures/metal-smoke/t28-output-ui.dds` 为 408 字节，SHA-256
  `ada9ab66b7c5…`，与自动参考 `t28_output.dds` 完全一致；T28 DDS 验收通过。
  用户确认 T22 父行展开为 EID 6/7，点父行绘制正常；T22 复验通过。
- Texture Viewer 右侧 Input/Output 列表有条目但缩略图为空：根因是通用
  `ReplayOutput::DrawThumbnail()` 使用 Headless output，Metal replay 原先仅接受
  macOS layer。现已为 Metal 增加 Headless output；最新 app 路径不变，GUI binary
  SHA-256 `b8259e2471ce…`、内嵌 replay 库 SHA-256 `81738a1bcbde…`。
  正式 T28/T29 captures 未变。T01/T03/T09/T11/T12/T16/T17/T22/T25/T28/T29
  的缩略图/Replay API 和逐份 CLI replay 通过；12 份 capture × 10 轮 lifecycle
  通过，resident growth 1,376,256 bytes；`git diff --check` 通过。T28/T29
  计算前黑色、计算后 Input/Output 有内容；T09 mip/array/cube 子资源也有内容。
  L3 条件未触发。用户仍需重启 app，在同轮 GUI 复验 T28/T29 右侧小图；
  T29 `$action()` 筛选亦待确认。详见 `QA_BATCH29-30.md` / `QA_PENDING.md`。
  T28/T29 与 BATCH29-30 不关闭，全部未提交改动保留。下一项等待用户 L4
  反馈；若反馈不符，先分析修复并给最短复验。

- 用户确认 T25 indexed instanced 的多个 instance、ICB 子 draw 位于 execute 内、
  点击 execute 画到最后子 draw；T27/T28 所见画面及 `setBuffer` 等状态 API 的
  EID 已确认。T25 本轮新 GUI 行为复验通过，T27 原验收保持通过。
- 本轮仍待人工确认：T28 从 10×7 Texture 20 保存的 GUI DDS（磁盘上旧文件仍
  是 400×300 Texture 30）；T29 `$action()` 隐藏 EID 10/11、保留 EID 12，
  清空筛选后恢复；T22 指定 EID 5 父行画出全部子项的画面。T28/T29 与
  BATCH29-30 继续未关闭。接手须看 `QA_PENDING.md`，不可因进入下一批遗漏。
- 已规划后续 `BATCH31-32.md`：T30 compute sampler 直接绑定、T31 compute
  texture/sampler/buffer 批量绑定；清单见两份 PHASE 文档。当前仅完成计划，
  未开始 T30 实现或宣称 T30/T31 自动/L4 通过。现有未提交改动全部保留。

## 2026-09-24 历史检查点：Metal 事件树与 ICB 复验

- 用户已确认 T29 Buffer 20 哨兵/写入变化，以及新版 API Inspector 可见 `setBuffer`
  等绑定。用户指出 Metal 无筛选 Event Browser 缺状态调用；ICB execute 子 draw
  未收在父行且点父行无绘制；范围 `0+1` 不清楚；indexed instanced 子行未显示实例数。
- 已为 Metal 帧内非 action 调用分配 EID；ICB execute 改为 MultiAction 父子层级，
  父行 seek 回放到范围末端；名称改为 `location=…, length=…`，子 draw 显示
  `instances=…`。API Inspector 的旧无 EID 补列路径已移除，现直接用正式 EID。
- 当前 qrenderdoc 路径仍为 `build-macos-debug/bin/qrenderdoc.app`，binary SHA-256
  `b8259e2471ce…`。正式 T28/T29 captures 已重生成；SHA-256 分别为
  `b54fb5128aaf…`、`99787263f8b9…`。T28 的 begin/dispatch/draw 是 EID 3/7/14；
  T29 的 fill/begin/setBuffer×2/dispatch/draw 是 EID 4/6/10/11/12/19。
- 自动结果：`/tmp/batch29-30-final.log` 的 T28/T29 native、capture/XML、Replay API、
  10 类异常、T11/T10/T01/T18/T19/T12/T16/T17、逐份 CLI 与 11×10 lifecycle
  再次通过，resident growth 1,343,488 bytes。事件语义受影响旧场景 T20–T27 的
  Replay API/CLI 逐份通过，T20–T27 异常拒绝按各自脚本通过，T00+T20–T27 的
  9×10 lifecycle 通过，growth 1,048,576 bytes。T29 两条 setBuffer EID 的逐项
  pipeline state、T22/T25 ICB 父行最终像素、层级、实例数均已定向断言。L0 构建与
  `git diff --check` 通过。事件影响已用编号定向覆盖，未触发 L3。
- **待人工 QA**：T28 Texture 20 的 10×7 UI DDS 仍待重存并由 agent 核对；T29
  Event Browser EID 10/11 与 `$action()` 筛选待验；T22/T25 因新 ICB 层级/父行
  seek/实例数显示改动需最短复验。新步骤与完整路径在 `QA_BATCH29-30.md` 顶部。
  T28/T29 与 BATCH29-30 不关闭；T22/T25 原已验功能保持关闭，只追踪新改动复验。
  全部未提交改动保留。第一项未完成工作是等待用户按当前验收单反馈；收到 DDS 后
  agent 须核对导出内容，并对任何不符先修复、重跑受影响自动验证、给最短复验步骤。

## 当前批次

当前计划批次为 `BATCH31-32.md`：T30/T31 尚未实现。上一批 BATCH29-30
已关闭。以下条目为 BATCH29-30 进行时的历史状态。

- 2026-09-24 已关闭 `BATCH27-28.md`：T26 ICB `inheritPipelineState` 与 T27
  `inheritBuffers` 的最终联合 L0/L1/L2、逐份 CLI replay、lifecycle 和同轮最新
  qrenderdoc L4 均通过；T00–T27 纵向切片已关闭，全部未提交改动保留。
- 当前批次：`BATCH29-30.md` / T28 `dispatchThreads` + T29 compute buffer binding；
  P29.1–P30.4 的功能与最终联合自动验证已通过。用户已反馈 T28 L4 部分通过，
  T29 L4 部分通过；当前以本文件顶部恢复检查点和 `QA_BATCH29-30.md` 的
  新 EID 验收步骤为准。下面旧 EID 与修复进程仅作历史记录。
  T28/T29 均不得标为关闭。
- 2026-09-24 T29 用户反馈与 API Inspector 修复：用户确认首次 ⌘O 重复弹窗已解决，
  T29 EID 4/5 的 Buffer 20 哨兵恢复和计算写入、画面及 `No problems detected`。
  `t29-pipeline-ui.html`、`t29-buffer-ui.csv` 和 Save Bytes 导出的 336-byte raw 已存在；
  raw 与自动 `t29_output.bin` 逐字节相同。UI 的实际菜单名是 Export to Bytes，
  对应原始字节；意外带反引号的原文件已复制为标准 `t29-buffer-ui.bin`，原文件保留。
  用户发现 API Inspector 看不到 `setBuffer`/`setTexture`。根因是这些状态 chunk
  存在于 XML，但 Metal 没为它们分配 EID，原 API Inspector 只列 action.events。
  已在 Metal action 的 API Inspector 中补列前一 EID 到该 action 之间的无 EID
  structured chunks，显示 `—` EID；事件树和 Replay API 语义未变。最新 GUI 构建
  `/tmp/batch29-30-api-inspector-build.log` 成功，binary SHA-256 `c1a773a05d56…`。
  用户须 ⌘Q 重开同一路径 app 后，在 T29 EID 5 最短复验两条 setBuffer 可见。
  EID 2/4 的 Buffer 20 值从 Resource Inspector 的 View Contents 打开 Buffer Viewer
  后切事件检查；EID 2 仍待用户明确确认。点击 API Inspector 中 Buffer 只到
  Resource Inspector 属预期。
  T28 的 10×7 Texture 20 UI DDS 仍待导出；批次未关闭。
- 2026-09-24 最新用户反馈与修复：T28 EID 2/5 等其余项目已由用户确认，
  `t28-pipeline-ui.html` 含正确的 `dispatchThreads`、`filter_main`、Texture 19/20。
  用户在 EID 1 看到的是默认 Texture 30 最终 backbuffer 渐变；此事件是 pass 边界，
  无 compute 绑定属预期，但仍需明确切到 10×7 Texture 20 检查全黑。
  用户保存的 `t28-output-ui.dds` 是 400×300 Texture 30，而目标 Texture 20 的自动
  DDS 为 10×7、408 bytes；需从 Texture 20 Viewer 重新保存并由 agent 核对。
  用户还报告每次冷启动首次 ⌘O 加载后文件选择窗口再次弹出。已将 ⌘O 的打开动作
  延后到按键事件结束，`/tmp/batch29-30-open-shortcut-build.log` 构建通过；最新 GUI
  binary SHA-256 `52c7e50b9928…`。正式 captures 未变，T28/T29 Replay API smoke、
  CLI replay 和自动 DDS/raw 对照再次通过。此 GUI 修复仍待用户首次打开实机复验。
  macOS 文稿/下载权限提示与当前位于 Developer 的 captures 路径不一致；本程序使用
  系统文件对话框，未见 app sandbox entitlement，具体权限触发路径尚无实机证据。
- 2026-09-24 首次打开问题二次修复：用户在相同 `.app` 上确认第一版延迟 ⌘O
  处理后仍可稳定复现：先开 T28，进度条和事件树出现后文件窗口再弹出，可继续开
  T29；反向顺序同样如此。源码中 Open Capture 同时注册为 Qt QAction 快捷键和
  `MainWindow::eventFilter` 全局快捷键；第二版只保留 Qt QAction 处理 ⌘O，
  第一版时序修改已撤回。`/tmp/batch29-30-open-shortcut-v2-build.log` 构建成功，
  GUI binary SHA-256 `969a565fe559…`。需用户 ⌘Q 退出旧进程后实机确认新版本。
  用户已找到 Texture List 并确认 T28 EID 1 Texture 20；UI DDS 仍为 400×300
  Texture 30，10×7 Texture 20 DDS 待导出。T29 未收到 GUI 反馈。
- 验收分工已更新：从 BATCH29-30 起，agent 完成全部终端可判定的 QA，并在最终
  captures 准备好后按 `QA_GUIDE.md` 给出一次性 T28/T29 GUI 验收单；用户反馈 L4
  通过后才关闭批次。`QA_PENDING.md` 跨批次保留所有未验 T；用户漏看或只反馈部分
  结果时其余项目持续“待人工 QA”，后续每次结果和交接都提示，且不得把对应批次
  标为关闭。当前 T28 的 GUI 已收到部分通过反馈，余项待最短复验；T29 尚待验。
  T26/T27 的 L4 已完成。
- 2026-09-24 T28/T29 已实现。最终构建为
  `build-macos-debug/bin/qrenderdoc.app`；正式 captures 为
  `captures/metal-smoke/t28_capture.rdc`、`t29_capture.rdc`。T28 7×5 thread grid/
  4×3 group 和 10×7 texture 未触及区；T29 buffer slot 2/4、offset 32/64、
  272-byte descriptor 范围与 64 项计算数据均由 Replay API 断言。
- 最终自动证据：脚本 `/tmp/run-metal-batch29-30.sh`，日志 `/tmp/batch29-30-final.log`。
  native 5 帧、正式 capture 8 帧/XML、T28 EID 1/2/5 与 T29 EID 4/5/8 的
  action/state/usage/readback/seek 和像素通过；T28 408-byte DDS、T29 336-byte raw
  内容已核对。T28/T29 合计 10 类非法 grid/binding/offset/resource 被拒绝。
  联合 T11/T10/T01/T18/T19/T12/T16/T17 Replay API/output 与 CLI 通过，
  11 份 capture × 10 轮 lifecycle resident growth 1,638,400 bytes；增量构建、
  脚本语法、Python 编译与 `git diff --check` 通过。随后仅调整 CS 页面空槽分类，
  `/tmp/batch29-30-ui-final-build.log` 重建 qrenderdoc 成功，正式 T28/T29 的
  Replay API smoke 再次通过；最终 GUI binary SHA-256 为 `15f84694baae…`。
- L3 决策：descriptor access 的改动已触发并通过 T12/T16/T17 条件验证；未改
  render-pass 或资源初始内容/所有权。定向验证未暴露无法圈定的跨场景风险，
  未到发布/合并门槛，故 T00–T29 L3 未触发。
- L4：`QA_BATCH29-30.md` 的合并验收单已更新。T28 仅 Texture 20 DDS 待导出；
  T29 的已确认 GUI 项及导出内容通过，API Inspector 无 EID 状态调用修复及
  EID 2 Buffer 20 值待最短实机复验。`QA_PENDING.md` 分别保留未验项，
  本批次保持未关闭。
- 用户指出原验收单没有讲清程序与两份 capture 的打开位置；已在
  `QA_BATCH29-30.md` 补上 Finder“前往文件夹”、qrenderdoc 打开顺序与三个完整路径。
  `QA_PENDING.md` 记录了此反馈；用户尚未执行 L4，T28/T29 继续待验。
- 最近自动证据：`/tmp/run-metal-batch27-28.sh`、`/tmp/batch27-28-final.log`，运行目录
  `/tmp/metal-batch27-28-final.XnLnvh`。T26/T27 native 5 帧、正式 capture/XML、Replay API
  action/state/usage/readback/seek、原始 buffer 字节、逐份 CLI replay 通过；联合
  T20/T22/T24/T25/T01/T05/T16/T19 定向 replay/output 与 CLI 通过。T20/T22/T24/T25
  的 6/11/8/15 类及 T26/T27 合计 14 类异常 RDC 被拒绝。11 份 capture × 10 轮
  lifecycle resident growth 507,904 bytes；最终增量构建、脚本语法、Python 编译、
  `git diff --check` 通过。
- 最近 L4 证据：最新 qrenderdoc 同一进程依次打开正式
  `captures/metal-smoke/t26_capture.rdc` 与 `t27_capture.rdc`。T26 两次 draw 的
  Pipeline State 17/18、Buffer 19、Mesh/Resource usage、红蓝输出；T27 的 Buffer 16/17、
  第二次 offset 16、IA/Mesh/Vertex Buffer usage、红蓝输出均正确。Event/API、资源跳转、
  HTML/CSV 导出和 DDS 保存通过。T26/T27 的 `*-pipeline-final.html`、
  `*-buffer-final.csv`、`*-output-final.dds` 位于 `captures/metal-smoke/`，导出内容已核对；
  两份 capture 均显示 `No problems detected`。
- 最近 L3 决策：indexed、fragment binding、render-pass 条件路径未改；联合定向验证未暴露
  无法圈定的跨场景风险，也未进入发布/合并门槛，故未运行 T00–T27 L3。最近完整
  L3 仍为 T00–T18 的 `/tmp/t18-final-regression.log`。
- 本批规则：终端可判定项已完成；仅待用户同轮 qrenderdoc L4。L3 条件未触发。

### 前批历史证据（BATCH25-26 关闭时）

- 2026-09-24 已关闭 `BATCH25-26.md`：T24 ICB reset 后重编码与 T25 混合命令 ICB 的最终
  联合 L0/L1/L2、逐份 CLI replay、lifecycle 和同一轮最新 qrenderdoc L4 全部通过；
  L3 未触发。T00–T25 纵向切片均已关闭，全部现有未提交改动保留。
- 最近自动证据：最终命令 `/tmp/run-metal-batch25-26.sh`，汇总
  `/tmp/batch25-26-final.log`，运行目录 `/tmp/metal-batch25-26-final.6n0AJS`。T24/T25 与
  T20/T22/T23/T01/T02/T13/T14/T21 的 native、capture/XML、Replay API/output、逐份 CLI
  replay 全部通过；T20/T22/T23/T24/T21/T25 的 6/11/18/8/9/15 类异常拒绝通过。
  含 T00 的 11 份 capture × 10 轮 lifecycle resident growth 1,212,416 bytes；脚本语法、
  Python 编译与 `git diff --check` 通过。
- 最近 L4 证据：最新 qrenderdoc 同一进程依次打开正式
  `captures/metal-smoke/t24_capture.rdc` 与 `t25_capture.rdc`。T24 的 `0+3` range、三个
  展开 draw、replacement 的 Pipeline/Mesh/Buffer/Resource、红/绿/蓝输出与 HTML/CSV 通过，
  旧紫色命令不可见。T25 的非索引/索引两个 action、无 index/UInt16 offset 4 size 6 的两套 IA、
  Mesh、Buffer/Resource、红/绿/蓝输出与 HTML/CSV/DDS 通过。两份均显示
  `No problems detected`。产物为 `t24-pipeline-final.html`、`t24-buffer-final.csv`、
  `t25-pipeline-final.html`、`t25-indexed-pipeline-final.html`、`t25-index-buffer-final.csv`、
  `t25-output-final.dds`。
- 最近 L3 决策：条件路径未触发；联合定向清单已覆盖 reset、混合 commandTypes、direct/
  indirect、indexed 和 ICB 公共路径，未见无法圈定的跨场景风险，也未进入发布/合并门槛，
  因此未运行 T00–T25 L3。最近完整 L3 仍为 T00–T18 的
  `/tmp/t18-final-regression.log`。
- 前一批 BATCH23-24 的自动证据：最终构建 `/tmp/t23-build.log`，联合命令
  `/tmp/run-metal-batch23-24.sh`，汇总 `/tmp/batch23-24-final.log`；T22/T23 与必跑
  T20/T21/T01/T02/T13/T14
  各自 5 帧 native、8 帧 capture、XML、Replay API、CLI replay 均通过。T20/T21/T22/T23
  的 6/9/11/18 类异常拒绝通过，9 份 capture × 10 轮 lifecycle resident growth
  1769472 bytes。T23 的 UInt16 offset 4、baseVertex/baseInstance 1、6-byte index raw 与
  左红右蓝输出通过。脚本语法检查与 `git diff --check` 通过；条件旧场景未触发，
  L3 未触发。
- 前一批 BATCH23-24 的 L4 证据：最新 qrenderdoc 同一进程依次打开与正式 capture SHA-256
  相同的 `/private/tmp/t22-batch-ui.rdc`、`/private/tmp/t23-batch-ui.rdc`。T22 的
  `1+2` range、EID 3/4 独立 Buffer 18/19、Mesh、ICB usage、红蓝输出和 HTML/CSV；
  T23 的 EID 3 indexed ICB、UInt16 index Buffer 18 offset 4/size 6、Buffer 16/17
  Vertex/Instance layout、Mesh 两实例、Index Buffer/ICB usage、红蓝输出和 HTML/CSV/DDS
  均通过。两份状态栏均为 `No problems detected`。T22 UI 产物
  `/private/tmp/t22-pipeline-final.html`、`/private/tmp/t22-buffer-final.csv`；T23 为
  `/private/tmp/t23-pipeline-final.html`、`/private/tmp/t23-index-buffer-final.csv`、
  `/private/tmp/t23-output-final.dds`，均已核对。
- 更早 BATCH21-22 自动证据：`/tmp/run-metal-batch21-22.sh` 对 T01/T02/T05/T13/T14/T20/T21 重跑
  native 5 帧、capture 8 帧、XML、Replay API smoke、逐份 CLI replay；T20 的 6 类、T21 的
  9 类异常 RDC 均被拒绝。含 T00 基线的 8 份 capture × 10 轮 lifecycle resident growth
  196608 bytes。汇总 `/tmp/batch21-22-final.log`，分项 `/tmp/batch21-22-*.log`。最终
  qrenderdoc Viewer 修复构建见 `/tmp/batch21-22-ui-fix-build.log`；修复只涉及 indirect 参数
  标签和 Buffer Viewer 格式，T13 Replay API smoke 复验通过，核心联合结果仍适用。
- 更早 BATCH21-22 L4 证据：最新 qrenderdoc 同一进程打开 SHA-256 与正式 capture 相同的
  `/private/tmp/t20-batch-ui.rdc`、`/private/tmp/t21-batch-ui.rdc`。T20 的 ICB marker/draw、
  IA/Mesh/Buffer/Resource、红色输出与 HTML/CSV 通过；T21 的 indexed action、6-byte index、
  20-byte 五字段参数、IA/Mesh/Buffer/Resource、红蓝输出与 HTML/CSV 通过。额外打开
  `/private/tmp/t13-batch-ui.rdc`，确认非索引 `Draw Primitives` 与四字段格式未回归。三份状态栏
  均为 `No problems detected`。产物 `/private/tmp/t20-batch-buffer-ui.csv`、
  `/private/tmp/t20-batch-pipeline-final.html`、`/private/tmp/t21-batch-arguments-final.csv`、
  `/private/tmp/t21-batch-pipeline-final.html`，内容已核对。
- 更早 BATCH21-22 L3 决策：条件触发的 T16/T19、T06/T07/T08 路径未变；通用 Viewer 修复的风险由
  T13/T21 UI 与 T13 自动状态界定，未见无法圈定的跨场景风险，也未进入发布/合并门槛，
  故未触发 T00–T21 L3。最近完整 L3 仍为 T00–T18 的 `/tmp/t18-final-regression.log`。

## 已完成

- RenderDoc 官方仓库已检出到工作区。
- 已核验标签 `v1.46`，基线提交为 `e4bd23b671d3d5a747ff5221dbe08a63eb6ca200`。
- 已创建本地开发分支 `metal-replay-v1.46`。
- 已确认仓库原有 Metal 骨架位于 `renderdoc/driver/metal`。
- 已完成 Metal 代码和本机工具链的第一轮静态盘点。
- 已建立计划、测试矩阵、决策和交接文档。
- 已安装 Qt 5.15.19、autoconf 2.73、automake 1.19、PCRE 8.45 和 bison 3.8.2。
- 已完成 `ENABLE_METAL=OFF` 的 qrenderdoc Debug 构建并验证主窗口。
- 已为 Xcode 26 / SDK 26 增加最小 Metal bridge 编译兼容补丁。
- 已完成 `ENABLE_METAL=ON` 的 qrenderdoc Debug 构建并重新验证主窗口。
- 已增加可重复构建/启动脚本 `util/buildscripts/scripts/build_metal_dev_macos.sh`。
- 已写好阶段 1 文件级计划 `PHASE1.md`。
- 已将 T00 `Metal_Empty_Frame` 和 T01 `Metal_Simple_Triangle` 接入 `util/test/demos`。
- 两个样例未注入 RenderDoc 时均能稳定运行；T01 的运行时 MSL 编译和三角形绘制通过。
- 修复 macOS 26 `CAMetalLayer` 内部 residency set 与代理 `MTLDevice` 不兼容导致的崩溃。
- 建立 drawable texture 的真实对象/RenderDoc wrapper 映射，render pass 可继续使用受跟踪纹理。
- 修复 app-controlled capture 在 present 时不记录 backbuffer、导致 `EndFrameCapture` 失败的问题。
- T00/T01 均能通过 `DYLD_INSERT_LIBRARIES` 和 in-app API 生成 Metal `.rdc`。
- T00 连续三次 capture 均成功，验证了最小链路稳定性。
- 注册 Metal structured processor；`renderdoccmd convert -c xml` 可解析 capture chunks。
- T01 structured XML 已核验 MSL、入口名、96 字节 vertex buffer、pipeline/binding、draw 和 present。
- 新增一键回归脚本 `util/buildscripts/scripts/test_metal_capture_macos.sh`。
- 已写好阶段 2 文件级计划 `PHASE2.md`。
- `MetalReplay` 已实现 `IReplayDriver` 并注册 `RDCDriver::Metal` replay provider。
- replay 初始化会创建真实 `MTLDevice`，并沿现有 structured 路径读取 capture。
- 已重建 T00/T01 所需的 device、queue、drawable 替代 texture、command buffer、render encoder、
  MSL library/function、pipeline、vertex buffer 和基础 render 命令。
- 已执行 T00/T01 的 clear、pipeline/buffer binding、`drawPrimitives`、commit/wait。
- 已建立 render pass、clear、draw、present 和 capture end 的最小 action/event。
- 已提供基础资源、buffer、texture 描述和 shared buffer 原始字节读取。
- `renderdoccmd replay --loops 1` 已分别对 T00/T01 成功运行并正常退出。
- qrenderdoc 已成功加载 `t01_capture.rdc`。此前的 degraded 弹窗来自 Metal replay 主动声明，
  并非 capture 加载失败或实际回退到软件渲染；完成 Texture Viewer 后已移除该标记。
- 已实现 macOS `CAMetalLayer` output window、尺寸/resize 跟踪、持久 BGRA8 output texture、clear、
  present 和 output readback。
- 已实现最小 fullscreen Metal texture display pipeline，支持 T00/T01 所需的 2D 单采样 color texture、
  fit/scale/pan、mip、flip、channel mask 和 range 映射。
- T00 output readback 得到预期 clear 色；T01 output readback 得到预期黑色背景和彩色三角形。
- qrenderdoc Texture Viewer 已实机显示 T01 三角形；窗口最大化后 resize/fit 仍正确，状态栏为
  `No problems detected`。
- Texture Viewer 闭环通过后已将 `APIProperties.degraded` 改为 `false`，新构建不再弹 degraded
  support 警告。
- `test_metal_capture_macos.sh` 现会编译离屏 output smoke、输出 T00/T01 PPM，并断言关键像素。
- 已保留 `CaptureBegin` 后的 frame stream，并为每个 API event 记录 frame-relative file offset。
- 已实现 T00/T01 所需的 `ReplayLog(Full/WithoutDraw/OnlyDraw)` 区间执行和未闭合 Metal
  encoder/command buffer 的安全收尾。
- 自动化会在同一个 replay controller 上连续 10 轮执行 T01 `clear -> draw -> clear`，中心像素依次为
  `#14141a -> #83645f -> #14141a`，验证前进和回退均会真实重放。
- qrenderdoc 实机切换 EID 1（clear）、EID 2（draw）、EID 1（clear）时画面分别为纯背景、彩色
  三角形、纯背景，状态栏始终为 `No problems detected`。
- 已实现 capture texture 的 `GetTextureData()`：通过 Metal blit 将单采样 2D RGBA8/BGRA8 mip
  复制到 shared buffer，并移除 Metal 行对齐 padding 后返回紧密排列的原始字节。
- 已基于同一 readback 路径实现 `PickPixel()`，按 RGBA/BGRA 通道顺序返回归一化浮点值；超出当前
  范围的 remap、resolve、MSAA、array/cube/3D、depth/stencil、压缩及其他格式均明确报 unsupported。
- T01 自动验证了 clear EID 的 400x300 BGRA backbuffer、背景字节 `1a1414ff`、背景拾取值约
  `(0.08, 0.08, 0.10, 1.0)`，以及 draw EID 中心像素不再是背景色。
- `ReplayController::SaveTexture()` 已使用同一路径成功保存 T01 为 400x300 ARGB8888 DDS，产物
  `captures/metal-smoke/t01_texture.dds` 为 480128 字节。
- 已把 `ShaderEncoding::MSL` 接入通用 replay/API 字符串与 qrenderdoc 源码高亮；source-created
  library 的源码会随 replay library 保存，并为 Metal function 生成真实 entry point、stage 和
  `ShaderReflection`，不为没有源码的 binary library 伪造 MSL。
- T01 自动断言恰有 `vs_main`/Vertex 与 `fs_main`/Fragment 两个 shader resource，两者均返回
  `captured.metal`、MSL encoding 和同一份包含真实入口声明的源码。
- qrenderdoc 已从 Resource Inspector 分别打开 Function 13/14；两个 Shader Viewer 均显示
  `captured.metal` 的真实 `metal_stdlib`、vertex/fragment 源码，状态栏保持 `No problems detected`。
- 修复 qrenderdoc 恢复到旧 D3D11 Pipeline State 子页面后加载 Metal capture 会错误调用 D3D11
  controller 并崩溃的问题：非 D3D/GL/Vulkan API 现在清空旧后端子页面，Metal capture 可稳定加载。
- 已新增独立 `MetalPipe::State`，并通过 replay controller、proxy serialization 和通用 `PipeState`
  暴露 T01 所需的 render pipeline、vertex/fragment shader、topology、vertex buffer 和 color target；
  不把 Metal 状态伪装成 D3D/GL/Vulkan。
- replay 会在 render pass、pipeline/buffer bind 和 draw 时更新 Metal snapshot；T01 自动断言
  `Pipeline State 15`、`Function 13/14`、`vs_main/fs_main`、`TriangleList`、96-byte `Buffer 16` 和
  swapbuffer `Texture 23` 均来自真实 replay 资源。
- qrenderdoc 已接入最小 Metal Pipeline State 页面。实机重新启动本次构建、加载最新 T01 并选择
  EID 2 后，页面显示上述 pipeline/shader/topology/VB/color target，状态栏为
  `No problems detected`；各资源行可进入 Resource Inspector。
- replay resource manager 现会确定性释放 wrapper 与真实 Metal 对象；wrapper 区分 `new*` 转移来的
  retained 对象和需要主动 retain 的 autoreleased command buffer/render encoder，并在重复 replay
  替换 live 对象时释放旧引用。ObjC embedded bridge 仅保留在 capture 路径。
- 新增 `metal_replay_lifecycle_smoke.mm`，在同一进程中打开/关闭 T00/T01 各 10 次并覆盖 action、
  texture readback 与 unsupported 接口；完整回归在两轮 warm-up 后 resident growth 为 491,520 字节，
  额外 50 轮压力检查增长 1,441,792 字节。
- shader debug 不再返回会被控制器解引用的空指针；四种 debug 调用返回可释放的空 trace。histogram、
  pixel history、post-VS 和 custom/target shader build 也返回稳定空结果或明确错误。
- 最新 qrenderdoc 同一进程完成 `T01 -> Close -> T00 -> Close -> T01`。重开后的 T01 EID 2 仍显示
  彩色三角形和完整最小 Pipeline State；T00/T01 状态栏均为 `No problems detected`，History/Debug
  按钮明确提示不支持并保持禁用。
- 已新增 `PHASE3.md`，将 T02 拆为 fixture、capture/resource、GPU replay/action、UI/data 和阶段关闭。
- 已新增确定性的 `Metal_Indexed_Cube`（T02）：一个 interleaved Float3 position/Float4 color vertex
  buffer、36 个 UInt16 index 和同内容的 UInt32 index、private `Depth32Float` attachment、less/write
  depth state，以及左右两个 viewport/scissor 的 indexed cube。
- 新增 `WrappedMTLDepthStencilState` 及 Objective-C bridge，序列化/重建
  `newDepthStencilStateWithDescriptor` 和 `setDepthStencilState`；descriptor 当前真实保存 label、depth
  compare 和 depth write，stencil face 明确保留为后续范围。
- 已接通 T02 所需的 `setScissorRect`、`setFrontFacingWinding`、`setCullMode` 和直接
  `drawIndexedPrimitives` capture/replay；indexed action 标记 `ActionFlags::Indexed` 并保存 count/offset。
- 修复 render pass 只引用 color attachment 的缺口：capture 现在统一追踪 color/depth/stencil 及
  resolve texture，因此 T02 private depth texture 的创建 chunk 会进入 `.rdc`，replay depth target 非空。
- 一键脚本现会原生运行并重新截取 T02，XML 断言 stride/attribute/depth/raster/scissor/index 参数，
  replay smoke 逐字节比较 72-byte UInt16 与 144-byte UInt32 index buffer，并检查左右半屏图像。
- P3.2 完整回归通过 T00/T01/T02 capture/replay 与三份 capture 各 10 次同进程开关；warm-up 后
  resident growth 为 1,196,032 字节，低于 64 MiB 阈值。
- 最新 qrenderdoc 已加载 T02，Event Browser 显示 EID 2/3 两个 `drawIndexedPrimitives(36)`；选中
  EID 3 后 API Inspector 标明 UInt32/Buffer 20，Texture Viewer 同时列出 color Texture 27 与 depth
  Texture 17，双立方体图像正确，状态栏为 `No problems detected`。
- `MetalPipe::State` 已扩展 vertex attributes/layout、index buffer、depth state 和 raster state，并通过
  通用 `PipeState` 暴露 index、viewport/scissor、depth/raster 查询；proxy serialization 同步覆盖。
- 修复 pipeline state 的 event 边界：加载 capture 时按 action event 保存 draw-time snapshot，避免
  `OnlyDraw` 区间继续执行到下一 event 前时把第一条 draw 的动态状态覆盖为第二条 draw 状态。
- T02 replay smoke 现逐 draw 断言 Float3/Float4、stride 28、UInt16/UInt32 binding、less/write、
  back/CCW、左右 viewport/scissor，并验证 clear -> draw1 -> draw2 -> draw1 回退的 BGRA 图像。
- 最新完整回归重新生成 T00/T01/T02 并全部通过；三份 capture 各 10 次 lifecycle 的 resident growth
  为 720,896 字节。最新 qrenderdoc 中 EID 2/3 Pipeline State 分别显示左/右 viewport 与
  `Buffer 19 / 72 / UInt16`、`Buffer 20 / 144 / UInt32`，其余 vertex/depth/raster/target 字段一致，
  状态栏为 `No problems detected`。
- `PipeState::GetVertexInputs()` 已映射 Metal attribute/layout，标准 Mesh Viewer 能按 T02 UInt16/UInt32
  index 展开 `attr0` Float3 与 `attr1` Float4；Metal output backend 可绘制当前 VS Input Float2/3/4
  点线/三角拓扑，自动 smoke 会检查 indexed cube 线框产生真实非背景像素。
- Pipeline State 的 vertex attribute 激活现进入 Mesh Viewer；vertex buffer 使用通用自动格式进入
  Buffer Viewer，index buffer 分别用 `ushort index`/`uint index` 查看精确子范围。Post-VS 输出表明确
  显示 Metal 不支持，不返回伪数据。
- P3.4/P3.5 最新完整脚本通过，三份 capture 各 10 次 lifecycle 的 resident growth 为 294,912 字节。
  最新 qrenderdoc 实机验证 EID 2 的 VS Input 表格/立方体线框、Buffer 18 interleaved 数据、Buffer 19
  UInt16 数据及 EID 3 Buffer 20 UInt32 数据，状态栏保持 `No problems detected`。
- 已新增确定性的 `Metal_Textured_Quad`（T03）：4x4 RGBA8 四色块纹理、Float2 position/UV、
  triangle strip 与 nearest/clamp sampler，原生运行、capture 和 structured XML 参数断言均通过。
- 已实现单层 2D `MTLTexture::replaceRegion` capture/replay，以及完整 wrapped sampler resource 的
  创建、序列化、重建和释放；`setFragmentTexture`/`setFragmentSamplerState` 会真实 replay 并更新状态。
- T03 replay smoke 精确验证 64-byte RGBA8 内容、四个 texel 的 `PickPixel()`、四象限 GPU 输出、
  `TriangleStrip`、Float2/Float2 input 和 fragment slot 0 的 texture/sampler。
- fragment texture/sampler 已经通过通用 descriptor API 暴露给 `PipeState`。Metal Pipeline 页面显示
  Fragment Textures/Samplers；texture 双击进入标准 Texture Viewer，sampler 进入 Resource Inspector。
- 最新 qrenderdoc 已加载 T03 EID 2，显示 Texture 17、Sampler 18（Point/Point/None、ClampEdge x3）和
  正确四色纹理；Texture Viewer/Resource Inspector 跳转均通过，状态栏保持 `No problems detected`。
- Metal Pipeline 页面已改用标准 Controls + `PipelineFlowChart` + 隐藏阶段页，形成 IA/VS/RS/FS/OM
  信息架构；`SelectPipelineStage()` 现在会切换对应页面，shader 行直接进入通用 Shader Viewer。
- `Show Empty Items` 会对已知空 binding/target/state 显示标准红色空槽；没有静态 shader resource
  reflection 的 pipeline 会禁用 `Show Unused Items` 并说明原因，避免将“已绑定”错误解释为“shader 使用”。
- 最终构建已分别用 T03/T02 实机核对新布局：T03 FS 显示 Texture 17/Sampler 18，IA/OM 空槽开关与
  shader 直接跳转正常；T02 IA、RS、OM 分别保持 UInt16 输入、左 viewport/scissor + Back/CCW、
  Texture 27 + Depth Texture 17 + Less/Write。两份 capture 状态栏均为 `No problems detected`。
- Metal Pipeline 的所有表已迁移到标准 `RDTreeWidget`/`RDHeaderView`，并通过父级
  `SetupResourceView()` 复用资源上下文菜单、usage、thumbnail/preview；Metal ResourceId 已补入三个
  通用分发点。最终 T03 FS 表双击 Texture 17 仍进入标准 Texture Viewer。
- 工具栏已加入标准 Export 控件；最终 qrenderdoc 在 T03 EID 2 实际写出
  `/tmp/metal-pipeline-t03.html`，文件包含 IA/VS/RS/FS/OM、Triangle Strip、Texture 17 和 Sampler 18。
  随后完整 T00-T03 回归通过，四份 capture 各 10 次 lifecycle resident growth 为 524,288 字节。
- replay 创建 Metal render pipeline 时会请求 argument reflection；T03 fragment shader 现枚举真实
  `colourTexture`/`colourSampler`、slot 0、类型、只读状态和 active 状态，并接入通用
  `ShaderReflection`/descriptor 查询。无反射证据的 pipeline 继续保守降级。
- T03 fixture 将同一 texture/sampler 额外绑定到 shader 未声明的 slot 1。自动回归验证 slot 0 映射
  reflection index 0 且为 used，slot 1 为 `NoShaderBinding + staticallyUnused`，`onlyUsed=true` 仅返回
  slot 0。最终 qrenderdoc 默认隐藏 slot 1；启用 `Show Unused Items` 后 texture/sampler 表均显示真实
  slot 1，状态栏为 `No problems detected`。最新完整 lifecycle resident growth 为 540,672 字节。
- 已新增 `Metal_Dynamic_Uniform`（T04）：单个 512-byte shared buffer 在 offset 0/256 保存两组固定
  float4 颜色，两条 fullscreen triangle draw 通过左右 viewport 输出红/绿半屏。
- 已完整 capture/replay `setFragmentBuffer` 与 `setFragmentBufferOffset`；structured XML 保存
  512-byte 初始内容、slot 0、offset 0/256 和两条 draw，事件回放验证 clear -> 左红 -> 左红右绿 ->
  左红往返。
- Metal pipeline state 新增 fragment buffer binding，并通过通用 constant-block descriptor/reflection
  枚举 `uniforms`、slot 0、16 bytes 和 active 状态；FS 页复用标准 RDTree 与 Buffer Viewer 跳转。
- 完整 T00-T04 一键回归通过；五份 capture 各 10 次 lifecycle resident growth 为 376,832 字节。
  最终 qrenderdoc 的 T04 EID 2/3 分别显示左红/右背景和左红/右绿，Constant Buffers 分别显示
  `Buffer 16 / 0 / 512` 与 `Buffer 16 / 256 / 256`。EID 3 双击进入 Buffer Viewer 的 offset 256、
  length 256 子范围，状态栏为 `No problems detected`；当前进程保持运行在 EID 3 FS 页。
- 已新增 `Metal_Instanced_Mesh`（T05）：24-byte Float2 position buffer 与 96-byte
  Float2 offset/Float4 colour instance buffer 分居 slot 0/1；直接 draw 使用
  `instanceCount=3/baseInstance=1` 输出红、绿、蓝三个固定实例。
- T05 structured XML、action 和 draw-time snapshot 均保存两个 vertex slot、stride 8/24、
  `PerVertex/PerInstance` step、instance count 3 与 base instance 1；自动回归逐字节验证两份 buffer，
  检查 clear/draw 往返、三处 `PickPixel()` 和最终 PPM `ff2010/10df30/1840ff`。
- 通用 `GetVertexInputs()` 与标准 Mesh Viewer 会应用 action 的 base instance：instance 0/1 分别显示
  offset `-0.55/0.00`，instance 1 colour 为 `.0625/.875/.1875/1.0`；VS Input 预览显示原始 attr0
  三角形，shader 变换后的 geometry 继续明确属于 post-VS unsupported。
- 完整 T00-T05 一键回归通过；六份 capture 各 10 次 lifecycle resident growth 为 573,440 字节，
  六份 capture 的 `renderdoccmd replay --loops 3` 与 `git diff --check` 均通过。最终 qrenderdoc 的
  T05 EID 2 正确显示三色实例，Pipeline IA 显示 Buffer 16/17 的 Vertex/Instance step，两个标准
  Buffer Viewer 与 Mesh Viewer 实例切换均正确，状态栏为 `No problems detected`；进程保持运行。
- 已新增 `Metal_MRT_Blend`（T06）：BGRA8 drawable 与 shared RGBA8 第二附件使用两条 draw；slot 0
  开启 `SourceAlpha/OneMinusSourceAlpha` RGB blending，slot 1 禁用 blending 并使用 RGB write mask。
- 修复 render pipeline color attachment capture 将 source RGB factor 错取为 source alpha factor 的
  字段错误；draw/clear/end-pass action 现保存全部 color outputs，Metal snapshot 与通用
  `GetColorBlends()` 返回每个 attachment 的 blend equation/write mask。
- Pipeline OM 页新增标准 RDTree Blend State 表，字段顺序对齐现有 GL/Vulkan 页面；Color Targets、
  Texture Viewer Outputs、资源激活与 HTML export 均复用公共路径，不增加 Metal 专用查看器。
- 完整 T00-T06 一键回归通过；七份 capture 各 10 次 lifecycle resident growth 为 1,015,808 字节，
  七份 capture 的 `renderdoccmd replay --loops 3` 与 `git diff --check` 均通过。最终 qrenderdoc 在 T06
  EID 3 显示 FB0 混合结果、FB1 RGB write-mask 结果以及两行正确 OM blend state；实际导出
  `captures/metal-smoke/t06_pipeline_state_standard.html`，状态栏为 `No problems detected`；进程保持运行。
- 已新增 `Metal_Depth_Stencil`（T07）：单一 `Depth32Float_Stencil8` attachment、两个 pipeline、两个
  depth-stencil state 和五条 draw，分别建立左右 stencil mask、通过 depth/stencil 的绿/蓝结果及一次
  可观察的 depth fail。
- 已完整保存/重建 front/back stencil compare、fail/depth-fail/pass operations、read/write masks 与
  single/dual dynamic reference；render-pass clear action 标记 `ClearDepthStencil`，draw action 保存真实
  `depthOut`，Metal snapshot/proxy serialization 与通用 `StencilFace` 同步更新。
- Pipeline OM 新增标准 `Stencil State` RDTree，并与 Depth Target/Depth State、Texture Viewer 和 HTML
  export 复用公共状态；共享 `PipelineFlowChart` 新增焦点及 Left/Right/Home/End 导航，继续向其他图形
  API 的标准交互收敛。
- 完整 T00-T07 一键回归通过；八份 capture 各 10 次 lifecycle resident growth 为 524,288 字节，八份
  capture 的 `renderdoccmd replay --loops 3` 均通过。最终 qrenderdoc 在 T07 EID 6 显示左绿右蓝输出、
  Texture 20、Less/Write Enabled 与正确 Front/Back stencil 状态；实际导出
  `captures/metal-smoke/t07_pipeline_state_standard.html`，状态栏为 `No problems detected`；进程保持运行。
- 已新增 `Metal_MSAA_Resolve`（T08）：4x BGRA8 multisample color attachment 显式 resolve 到 drawable，
  三条 draw 形成左红、右蓝与中央绿色叠加三角形；pipeline 同时覆盖 sample count 和
  alpha-to-coverage。
- replay texture description 现在区分 `Texture2DMS`；draw-time snapshot 保存 sample count、
  alpha-to-coverage/one、MSAA color target 与单采样 resolve target，action output 指向可显示的真实
  resolve 资源，不伪造 MSAA readback。
- Pipeline OM 新增标准 `Multisample State` 与 `Resolve Targets` RDTree，Color Targets 增加 Samples；
  Resolve Targets 复用标准 Texture Viewer 跳转，五阶段 HTML export 输出同一状态。
- 完整 T00-T08 一键回归通过；九份 capture 各 10 次 lifecycle resident growth 为 376,832 字节，九份
  capture 的 `renderdoccmd replay --loops 3` 均通过。最终 qrenderdoc 在 T08 EID 4 显示
  `Texture 17 / Texture 2D MS / 4 samples`、`Texture 24 / Texture 2D / 1 sample` 和红绿蓝 resolve
  图像，中心拾取为 `(0.06275, 0.87451, 0.18824, 1.00)`；实际导出
  `captures/metal-smoke/t08_pipeline_state_standard.html`，状态栏为 `No problems detected`；进程保持运行。
- 已新增 `Metal_Texture_Subresources`（T09）：3-mip RGBA8 2D、3-slice 2D array 和六面 cube；
  12 个固定色子资源通过 12 条屏幕色带采样，native 输出与 capture structured XML 均通过。
- 新增 slice-aware `replaceRegion` capture/replay chunk，纹理 descriptor 保留 cube/array 语义；
  `GetTextureData()`、`PickPixel()` 和标准 output renderer 按 mip/slice/face 读取、显示并拒绝越界组合。
  自动 smoke 逐一检查 12 个原始子资源、六面 cube pick、每个 display、12 条色带和 512-byte cube DDS。
- T09 EID 2 的 Pipeline FS 显示 Texture 17/18/19（2D/2D Array/Cube）与 Sampler 20；标准 Texture
  Viewer 实机显示 2D mip0/1/2 红/绿/蓝、array slice0/1/2 黄/品红/青与 cube face X+/X-/Z- 的
  不同固定色。Qt 5/macOS 26 的 combo popup 会在 Cocoa 插件崩溃，两个子资源选择控件改为点击
  循环、键盘方向键/Home/End 选择，实机切换与状态栏均通过。
- 完整 T00-T09 一键回归通过；十份 capture 各 10 次 lifecycle resident growth 为 475,136 字节。
  最新 qrenderdoc 从 T09 EID 2 的 cube Z- 保存全部 faces 到
  `captures/metal-smoke/t09_cube_ui.dds`，与自动保存产物均为 512-byte DDS 且 `cmp` 完全一致；状态栏为
  `No problems detected`。因 macOS `Documents` 目录下的 capture 直接打开偶发阻塞，L4 使用
  同一最新 `.rdc` 的字节拷贝 `/tmp/t09-ui-capture.rdc`；qrenderdoc 保持运行。
- 已新增 `Metal_Blit_Operations`（T10）：固定 64-byte shared buffer 执行 offset 8 到 0 的
  32-byte copy，并对 destination 16..31 fill `0x60`；8x8 RGBA8 四象限 texture 执行 texture copy，
  另一张 4-mip texture 生成 mip chain。最终 draw 分四条色带独立采样四种结果，native 自检通过。
- 已补齐 blit encoder 创建/end、buffer copy/fill、texture region copy 和 mipmap generation 的
  capture/replay；buffer offset/length 与 texture mip/slice/origin/size/format 均在执行前验证。每个操作
  生成独立 action/event，并通过 `CopySrc`/`CopyDst`/`Clear`/`GenMips` usage 接入通用资源路径。
- T10 定向 smoke 已核对 structured XML、event 顺序与资源链接、copy/fill 前进和回退、buffer/texture
  原始数据、mip1/mip3、最终四条色带和 468-byte 全 mip DDS。T03/T09 texture 定向 replay/output 与
  T00/T03/T09/T10 各 10 次定向 lifecycle 均通过。
- 最终代码的 T00-T10 一键回归复验通过；十一份 capture 各 10 次 lifecycle resident growth 为 1,277,952
  字节，逐份 `renderdoccmd replay --loops 1` 均通过。最新 qrenderdoc 的 Event Browser 显示
  `Copy/Clear Pass #1` 和 EID 1-6 blit 子事件；Resource Inspector 的 Buffer 18 为 EID 2
  `Copy - Dest`、EID 3 `Clear`，Texture 21 为 EID 5 `Generate Mips`。Buffer Viewer 显示
  copy/fill 前后字节，Texture Viewer 显示 Texture 20 四象限、Texture 21 mip3 拾取
  `(134,132,88,255)` 和 EID 8 四条结果色带；UI/自动两份 468-byte DDS 逐字节一致，
  状态栏为 `No problems detected`。D034 修正的组名与 usage 文案已用最新 app 包内库实机确认。
- 已新增 `Metal_Compute_Texture_Filter`（T11）：8×8 RGBA8 source/destination、`filter_main`、
  `2×2×1` threadgroups / `4×4×1` threads/group，B/R/G swizzle 后由 render draw 采样。
  compute pipeline/encoder wrapper、capture/replay chunk、dispatch action/usage、compute shader reflection、
  标准只读/读写 descriptor 与 Metal Pipeline CS 页均已接通。
- `/tmp/t11-final-regression.log` 记录 T00-T11 原生运行、重新 capture、XML、输出 smoke、逐份 CLI
  replay 和 12×10 lifecycle 全部通过，resident growth 999,424 字节。T11 smoke 验证 dispatch
  前全零、后全部 256 字节 CPU swizzle、回退再前进、最终画面及 384-byte DDS；T03/T09/T10 定向输出
  验证也通过。
- 最新 qrenderdoc 加载与正式 capture SHA-256 一致的 `/tmp/t11-ui-capture-final.rdc`；Event Browser
  显示 EID 1-3 Compute Pass、EID 2 dispatch 和 EID 5 final draw。CS 页显示 Compute Pipeline
  State 16 / Function 13 `filter_main`、slot 0 Texture 19 只读、slot 1 Texture 20 读写；资源跳转进入
  标准 Texture Viewer，Resource Inspector 分别显示 `CS - Texture`/`CS - Image/SSBO`。
  EID 1 目标纹理全黑，EID 2 的首像素拾取约 `(0.06275,0.12549,0.09412,1.0)`；EID 5 的
  final draw 采样 Texture 20，画面与参考一致。UI DDS `captures/metal-smoke/t11_filtered_ui.dds`
  与自动 DDS 均为 384 字节且 `cmp` 一致；`/tmp/t11_pipeline_state_standard.html` 包含 shader、
  Texture 19/20。状态栏为 `No problems detected`。
- 已完成 T12 单层 fragment argument buffer 与 T13 单次非索引 indirect draw。T13 使用 48-byte
  shared buffer 中 offset 16 的 16-byte `3/2/1/1` 参数，原生和 replay 左橙右蓝图像一致；
  action、usage、Pipeline `Indirect Buffer`、标准 Buffer Viewer、Resource Inspector 和 raw `.bin`
  均指向 Buffer 18。offset 17/36 派生 RDC 分别验证未对齐/越界拒绝。
- `/tmp/t13-final-regression.log` 记录 T00-T13 native/capture/XML/output/data/state、逐份 CLI replay
  与 14×10 lifecycle 通过，resident growth 1,294,336 bytes。最新 qrenderdoc 实机显示 EID 2
  indirect draw、16/16 参数区和 `3/2/1/1`；UI/自动 `.bin` 逐字节一致，Pipeline HTML 包含
  Indirect Buffer，状态栏为 `No problems detected`。
- T14 `Metal_Indexed_Instancing` 使用 UInt16 index buffer 的 byte offset 4、baseVertex 1、
  baseInstance 1、两个实例；位置/实例哨兵使三字段任一失效都无法得到左红右蓝输出。新 overload
  已完成 bridge、capture chunk、序列化、GPU replay、`Indexed|Instanced` action、精确 6-byte index
  binding、vertex/index usage、标准 Mesh/Buffer Viewer。XML、buffer 数据、clear/draw/回退像素和
  raw index export 自动断言通过；offset 3（未对齐）与 10（越界）的派生 RDC 均被 replay 拒绝。
- `/tmp/t14-final-regression.log` 记录 T00-T14 native/capture/XML/output/data/state、逐份 CLI replay
  和 15×10 lifecycle 全部通过，resident growth 1,015,808 bytes。最新 qrenderdoc 实机打开正式
  `t14_capture.rdc`，Event/API EID 2、IA Buffer 16/17/18、Index Buffer offset 4/length 6、
  Resource Inspector `Index Buffer` usage、Mesh instance 0/1、左右像素、标准 HTML/CSV 导出均正确；
  状态栏 `No problems detected`。产物 `captures/metal-smoke/t14_indices.bin`、
  `t14_indices_ui.csv`、`t14_pipeline_state_standard.html` 均已验证。
- T15 `Metal_Point_Line` 的 Point `(start=1,count=1)`、Line `(3,2)`、Line Strip `(6,3)`
  使用 264-byte interleaved Float2/Float4 buffer 和视口外哨兵。原生 BGRA 像素、RDC XML、
  action/topology、VS Input、Buffer 字节、Mesh preview、Vertex Buffer usage、clear/draw/回退和
  480128-byte DDS 均通过；非法 primitive 99 与零 vertexCount 派生 RDC 被 replay 拒绝。
- `/tmp/t15-final-regression.log` 记录 T00-T15 原生/capture/XML/output/data/state、逐份 CLI replay
  及 16×10 lifecycle 全部通过，resident growth 737280 bytes。正式
  `captures/metal-smoke/t15_capture.rdc` SHA-256 为
  `208b794cc2ab49cf88f970cdda787bfb016f3483284b389130e05a39bae1a614`。
  最新 qrenderdoc 完全重启后实机核对三种 action 与 EID 2/3/4 拓扑、Line Strip API 参数、
  Point/Line/Line Strip Mesh VS Input、标准 Buffer Viewer 全部顶点值、Resource Inspector
  `Vertex Buffer`、Texture Viewer 红点绿线蓝折线、Pipeline HTML Line Strip/Buffer 16，状态栏
  `No problems detected`。UI/自动 DDS 均为 480128 bytes，SHA-256 同为
  `4bedcfdcddf745e379a2f693eac75a5dedec53df32ffba7ddad9011f60de0a5c`；
  产物为 `t15_output_ui.dds`、`t15_pipeline_state_standard.html`。
- T16 `Metal_Vertex_Texture` 使用 384-byte Float2/Float2 buffer、2×2 RGBA8 texture 与
  Point/Clamp sampler，在 vertex shader 采样并输出四象限纯色。原生 readback、XML、直接
  vertex texture/sampler bridge/chunk/frame reference/GPU replay、event snapshot、VS reflection/
  descriptor、`VS_Resource` usage、资源字节、VS Input、clear/draw/回退及 DDS 自动断言均通过。
  T03/T12 fragment 定向 smoke 通过；slot 128 与空 texture/sampler 的四份派生 RDC 在对应
  chunk 被拒绝。
- `/tmp/t16-final-regression.log` 记录 T00-T16 原生/capture/XML/output/state、逐份 CLI replay
  和 17×10 lifecycle 全部通过，resident growth 114688 bytes。最新 qrenderdoc 实机使用与
  `captures/metal-smoke/t16_capture.rdc` SHA-256 同为
  `2f1b95128946d180d7f114ca9047bb2b90f28d43915e82ad9eca2925ff788ec5` 的
  `/tmp/t16-ui-capture.rdc`：Event/API EID 2、VS Texture 17/Sampler 18、Mesh VS Input、
  384-byte 标准 Buffer Viewer、Texture 17/Resource Inspector `VS - Texture`、四象限输出与
  状态栏 `No problems detected` 均通过。HTML 为
  `captures/metal-smoke/t16_pipeline_state_standard.html`；UI/自动 480128-byte DDS 的
  SHA-256 同为 `95597aa5bcb91a27f057df2f4f36cbeb49423ac7e5275d5ed17ca29b03ca21cf`。
  验证命令：`bin/demos_x64 Metal_Vertex_Texture --frames 5`；
  `build-macos-debug/metal_replay_output_smoke captures/metal-smoke/t16_capture.rdc /tmp/t16-directed.ppm /tmp/t16-directed.dds`
  （T03/T12 使用相同 smoke 的对应 capture）；
  `RENDERDOC_METAL_CAPTURE_DIR=/tmp/t16-final-regression-captures util/buildscripts/scripts/test_metal_capture_macos.sh > /tmp/t16-final-regression.log 2>&1`。
- T18 `Metal_Fragment_Storage_Buffer` 使用 640-byte shared buffer，在 byte offset 256 的
  fragment `device const float4 *` 读取四组颜色，物理 slot 3，未绑定 slot 5。native BGRA、
  capture/XML、storage reflection/descriptor、`PS_Resource` usage、事件 seek、640-byte raw export、
  哨兵和异常 offset 均经自动 smoke 验证；T04/T12/T10 定向通过。
- `/tmp/t18-final-regression.log` 记录 T00-T18 全量原生/capture/XML/output/state、逐份 CLI replay
  与 19×10 lifecycle 全部通过，resident growth 1,556,480 bytes；`git diff --check` 通过。
  最新 qrenderdoc 加载与正式 capture SHA-256 同为
  `4e8436d6c9859c9dcf8ba57273dd9b1515cfa3f641339bf6219ba2ffec247aa8` 的
  `/tmp/t18-ui-final.rdc`，EID 2 的 FS Storage Buffers slot 3/offset 256/size 384、
  标准 Buffer Viewer 四组值、Resource Inspector `FS - Resource`、HTML/CSV 保存与状态栏
  `No problems detected` 均通过。UI CSV 前四行与自动 raw export 对应字节一致。
  产物位于 `captures/metal-smoke/t18_capture.rdc`、`t18_storage.bin`、
  `t18_storage_ui.csv`、`t18_pipeline_state_standard.html`。UI 二进制菜单项未单独点击；
  raw 内容由自动 smoke 验证。
- T19 `Metal_Vertex_Storage_Buffer` 使用 768-byte shared buffer，在 byte offset 256 保存 24 个
  `float4` 位置；vertex shader 从 slot 4 读取，slot 6 绑定相同资源但静态未使用。自动 smoke 核对
  四象限 native/replay、XML、reflection/descriptor、`VS_Resource`、IA/storage 分类、
  clear/draw/回退、完整 raw 和异常 slot/offset。最终 T19/T18/T16/T02/T05 定向、CLI replay、
  T19 10 轮 lifecycle 和 `git diff --check` 通过；日志为 `/tmp/t19-final-t19.log`、
  `/tmp/t19-final-t18.log`、`/tmp/t19-final-t16.log`、`/tmp/t19-final-t02.log`、
  `/tmp/t19-final-t05.log`、`/tmp/t19-final-cli.log` 与 `/tmp/t19-final-lifecycle.log`。
  分类公共路径风险已由 T02/T05 覆盖，未触发 T00-T19 L3。最新 qrenderdoc L4 核对 EID 2、
  `drawPrimitives(24)`、IA 空表、VS Storage Buffers slot 4/6、256+512 Buffer 范围、
  Resource Inspector `VS - Resource`、四象限、HTML/CSV/bin 与 `No problems detected`；
  512-byte UI bin 与自动 raw 对应子范围一致。

## 已验证环境

| 项目 | 当前值 | 结论 |
| --- | --- | --- |
| macOS | 26.1 / Build 25B5042k | 目标主机 |
| 架构 | arm64 | Apple Silicon |
| Xcode | 26.0.1 / Build 17A400 | 高于仓库最低要求 12.2 |
| macOS SDK | 26.0 | 需要留意 v1.46 与新 SDK 的兼容差异 |
| CMake | 4.4.3 | 高于 Apple 构建最低要求 3.23 |
| Ninja | 1.13.2 | 可用 |
| Qt 5 qmake | 5.15.19 | 已验证 |
| autoconf | 2.73 | 已验证 |
| automake | 1.19 | 已验证 |
| PCRE | 8.45 | 已验证；CMake 当前仍选择本地构建副本 |
| bison | 3.8.2 | 已验证 |

Qt 5 会警告它只测试到 macOS SDK 14，当前 SDK 26 属于 Qt 未验证组合，但实际编译和窗口启动已
通过。SWIG 配置期间 macOS 打印过缺少 Java Runtime 的提示，后续 SWIG build/bindings 生成仍成功。

## 代码基线发现

- `renderdoc/driver/metal` 顶层源文件总计约 15,395 行。
- `METAL_NOT_HOOKED()` 约 237 处。
- TODO/FIXME/未实现/未处理类标记约 402 处。
- `MetalReplay` 已继承 `IReplayDriver`，但大量进阶接口仍明确返回 unsupported/空结果。
- Metal replay provider 已注册，T00/T01 可进入真实 GPU replay 初始化。
- `WrappedMTLDevice::ProcessChunk()` 只覆盖一小部分 device/resource/render encoder chunk。
- `WrappedMTLDevice::AddAction()` 和 `AddEvent()` 已接入最小 frame record。
- qrenderdoc 已有 macOS `CAMetalLayer` 输出窗口适配，能作为后续 replay output 的基础。

## 当前阻塞与风险

1. Qt 5.15 对 SDK 26 给出未验证警告；原生 combo popup 在 Cocoa 插件崩溃。T09 的 mip/slice/face
   控件已用 macOS 点击循环和键盘选择避开 popup，其他 combo 仍需后续 UI 回归关注。
2. SDK 26 新增的 Metal 协议方法目前由 Objective-C forwarding 转交真实对象，尚未被 capture。
3. Metal replay 不是局部补丁：接口注册、事件模型、资源读取、状态快照和输出都需要实现。
4. Texture Viewer 和 texture readback 当前支持单采样 RGBA8/BGRA8 2D，以及 T09 的 RGBA8
   mipmapped 2D、2D array 和 cube；T08 可显示单采样 resolve 结果。逐 sample/MSAA attachment、
   cube array/3D、depth/stencil、整数、浮点和压缩格式 readback 仍明确不支持。
5. 当前 drawable hook 只验证了本机 `CAMetalDrawable` 具体类；后续需要覆盖多屏/不同 GPU 可能出现
   的其他 drawable class。
6. 当前 event-range replay 已覆盖 T00-T13 的单 command buffer，并包含 T10 blit→render、
   T11 compute→render 与 dispatch 前后/回退纹理字节、
   T02 indexed、T04 dynamic offset、T05 instanced draw、T06 双附件 blend、T07 depth/stencil
   五 draw 与 T08 三次 MSAA resolve draw 的前进/回退专项断言；多 command buffer、多 render
   pass、嵌套 debug group 和
   load-action initial contents 仍需按后续样例扩展。
7. `OnlyDraw` 遵循 RenderDoc 控制器约定，依赖紧邻的 `WithoutDraw` 建好同一 encoder 的前置状态；
   当前不承诺把 `OnlyDraw` 当作独立入口调用。
8. T00-T13 的 replay wrapper/Metal object 释放已经完成并通过完整循环测试；后续 wrapper 必须继续
   遵守 D017 的 transferred/retained 所有权规则，避免重新引入双重释放或泄漏。
9. 当前 Metal Pipeline State 承诺 T01 基础字段、T02 的 Float3/Float4 vertex descriptor、UInt16/32
   index/depth/raster、T03 fragment texture/sampler、T04 fragment constant buffer/dynamic offset，
   T05 多 vertex buffer/per-instance layout、T06 多 color target/逐 attachment blend state、T07
   combined depth/stencil/front-back stencil state、T08 multisample/resolve/sample state 与 T09 的
   fragment 2D/array/cube texture bindings，以及 T11 的 compute pipeline/shader 和直接读写
   2D texture bindings，以及 T16 直接 vertex、T17 VS/FS 批量 texture/sampler、T18 fragment
   storage buffer 和 T19 vertex storage buffer bindings；其他 vertex format、writable/array buffer、
   更多 blend/depth-stencil 组合仍归后续范围。
10. 直接非索引 draw 已覆盖 instance count/base instance 与 T13 shared buffer 间接参数；indexed draw
    已支持 T14 直接 indexed instancing/base vertex/base instance。T20/T22/T23 已覆盖 CPU 编码
    单命令、多命令和 indexed ICB；T21 已覆盖 CPU/shared 五字段 indexed indirect；T24/T25 已覆盖
    reset 后重编码与混合命令 ICB；T26/T27 已覆盖 pipeline/buffer inheritance。GPU 生成、compute
    ICB、heap、blit ICB 管理和多 queue 仍不在当前支持承诺中。
11. 当前 Metal mesh renderer 只承诺 VS Input 的 Float2/Float3/Float4 和 Metal 可直接绘制的常见
    point/line/triangle topology；T05 的 per-instance 表格读取已支持，但 raw VS Input preview 不推导
    shader 中的 instance transform。post-VS、选点、高亮、solid/secondary/bbox 等仍待后续实现。
12. Metal Pipeline State 已接入标准 IA/VS/RS/FS/OM/CS、empty-slot、RDTree 资源操作/预览、HTML export、
    有反射证据的 used/unused 过滤、fragment constant buffer、逐附件 blend 与 depth/stencil；共享阶段
    导航已支持 Left/Right/Home/End。T16/T17 补齐 VS/FS texture/sampler 直接与批量表，T18/T19
    补齐 fragment/vertex storage buffer 表；剩余差异是更细的紧凑布局、更多状态字段及高级绑定类型。

## 下一步（按顺序）

1. 下一开发项为 `PHASE31.md` P31.1：T30 compute sampler fixture/native。
   完成 T30 必要自动验证后按 `BATCH31-32.md` 连续推进 T31；批末向用户交付
   合并 GUI 验收单。T30/T31 尚未实施，当前 `QA_PENDING.md` 无待验项。

## 恢复检查点

- 2026-09-24 T29 用户反馈检查点：首次 ⌘O 重复弹窗已由用户确认修复。
  T29 EID 4/5 Buffer 20 值回退/前进、画面、状态栏通过；HTML/CSV/336-byte Save Bytes
  均已导出，raw 与自动参考完全一致。API Inspector 看不到状态调用，原因是 Metal
  `setBuffer` 等 chunk 无 EID，原组件只显示 action.events。已补列该 action 前的
  structured chunks，标记无 EID；最新 qrenderdoc 构建成功，SHA-256
  `c1a773a05d56…`，`git diff --check` 通过。第一项未完成：用户 ⌘Q 后在新 GUI
  的 T29 EID 5 核对两条 setBuffer，并确认 EID 2 Buffer 20 全 `a5`；T28
  Texture 20 10×7 UI DDS 仍待导出。
  正式 captures 不变，全部未提交改动保留，两个阶段与批次均未关闭。
- 2026-09-24 首次打开二次修复检查点：用户确认第一版 ⌘O 延迟触发修复无效，
  仍在 T28/T29 任意先后顺序中打开第一份后再次弹文件窗口。修改
  `qrenderdoc/Windows/MainWindow.cpp`，让 Open Capture 只走 Qt QAction 快捷键，
  不再在 ShortcutOverride 全局表中重复触发；第一版修复已撤回。增量构建通过，
  最终 GUI SHA-256 `969a565fe559…`，正式 captures 未变。下一项：用户 ⌘Q 后
  重开同一路径 app，确认首次 ⌘O 只弹一次。用户已完成 T28 EID 1 Texture 20
  目视验收；10×7 UI DDS 仍待导出，T29 L4 待验。无失败构建命令；未提交改动
  全部保留，批次未关闭。
- 2026-09-24 T28 用户反馈检查点：用户确认 T28 其余 L4 项通过；EID 1 看到了
  Texture 30 而非目标 Texture 20，UI DDS 亦为 Texture 30。T29 未收到反馈。
  修复首次 ⌘O 可能因嵌套事件循环重入而重复弹窗的问题，仅修改
  `qrenderdoc/Windows/MainWindow.cpp`：延迟打开动作。增量构建、T28/T29 Replay API
  smoke 与 CLI replay、自动导出内容对照、`git diff --check` 通过。新 GUI SHA-256
  `52c7e50b9928…`；正式 capture SHA 未变。无失败命令。第一项未完成：用户使用
  新构建按 `QA_BATCH29-30.md` 最短复验首次打开、T28 Texture 20 EID 1/DDS；随后
  同轮验 T29。L3 未触发；全部历史未提交改动保留。不可将两阶段或批次标为关闭。
- 2026-09-24 BATCH29-30 自动验证完成检查点：最终代码/GUI 构建成功，正式 T28/T29
  capture 与自动证据见顶部和 `BATCH29-30.md`。T28/T29 自动通过，用户 L4 尚未反馈；
  `QA_PENDING.md` 保留两项待验。最后成功命令 `bash /tmp/run-metal-batch29-30.sh`，
  日志 `/tmp/batch29-30-final.log`；随后 DDS/raw 内容、正式 SHA-256、合并验收单核对。
  无当前失败命令。全部历史未提交改动保留。下一安全操作是交付
  `QA_BATCH29-30.md` 给用户；收到反馈前不关闭两阶段。本轮 L1/L2/CLI/lifecycle 已完成，
  L3 未触发，L4 待用户同轮完成。

- 2026-09-24 BATCH27-28 关闭检查点：最终脚本 `/tmp/run-metal-batch27-28.sh` 与汇总
  `/tmp/batch27-28-final.log` 通过。T26/T27 及联合 T20/T22/T24/T25/T01/T05/T16/T19
  的 native/capture 或定向 replay、逐份 CLI、异常拒绝和 11×10 lifecycle 均通过；
  resident growth 507,904 bytes。同一最新 qrenderdoc 进程验收正式 T26/T27 capture，
  逐 draw Pipeline/Buffer/offset、Mesh/Resource、红蓝输出、HTML/CSV/DDS 保存均通过，
  两份均 `No problems detected`。L3 条件未触发。`BATCH27-28.md`、`PHASE27.md`、
  `PHASE28.md` 与索引文档已同步，全部 dirty 改动保留。下一批 `BATCH29-30.md`
  分为 `PHASE29.md` T28 dispatchThreads 与 `PHASE30.md` T29 compute buffer binding；
  第一项未完成 P29.1。最近完整 L3 仍是 T00–T18。

- 2026-09-24 BATCH25-26 关闭检查点：最终脚本 `/tmp/run-metal-batch25-26.sh` 与汇总
  `/tmp/batch25-26-final.log` 通过。T24/T25 及联合 T01/T02/T13/T14/T20/T21/T22/T23
  native/capture/XML/Replay API/逐份 CLI replay、6/11/18/8/9/15 类对应异常拒绝及
  11×10 lifecycle 全部通过，resident growth 1,212,416 bytes。同一最新 qrenderdoc 进程
  依次验收正式 T24/T25 capture：T24 reset/reencode 的三个展开 draw、旧命令失效、
  IA/Mesh/Buffer/Resource、HTML/CSV、红绿蓝输出；T25 两类 action、direct 无 index、indexed
  UInt16 4/6 与 Vertex/Instance 输入、Mesh/Buffer/Resource、HTML/CSV/DDS、红绿蓝输出；
  两份均 `No problems detected`。条件旧场景与 L3 均未触发。`BATCH25-26.md`、
  `PHASE25.md`、`PHASE26.md` 与索引文档已同步，全部未提交改动保留。下一批已拆为
  `BATCH27-28.md`、`PHASE27.md` T26 pipeline inheritance、`PHASE28.md` T27 buffers
  inheritance；第一项未完成为 P27.1。最近完整 L3 仍是 T00–T18。

- 2026-09-24 BATCH23-24 关闭检查点：T22/T23 最终联合 T01/T02/T13/T14/T20/T21/T22/T23
  native/capture/XML/Replay API/逐份 CLI replay、T20/T21/T22/T23 的 6/9/11/18 类
  异常拒绝及 9×10 lifecycle 全部通过，汇总 `/tmp/batch23-24-final.log`。同一最新
  qrenderdoc 进程依次验收哈希与正式文件相同的 T22/T23 capture：T22 `1+2` range、
  两个展开 draw/IA/Mesh/Buffer/Resource、HTML/CSV、红蓝输出；T23 indexed action、
  UInt16 index 4/6、两实例 Mesh、Buffer/ICB usage、HTML/CSV/DDS、红蓝输出；两份均
  `No problems detected`。L3 未触发，条件旧场景未触发。`BATCH23-24.md`、
  `PHASE23.md`、`PHASE24.md` 与索引文档已同步，全部未提交改动保留。
  下一批 `BATCH25-26.md` 已拆为 `PHASE25.md` T24 reset 后重编码、`PHASE26.md`
  T25 混合命令 ICB；第一项未完成为 P25.1。最近完整 L3 仍是 T00–T18。

- 2026-09-24 T23 P24.4 批末 UI 中断检查点（已由上方关闭检查点取代）：最终联合 L0/L1/L2、逐份 CLI replay、
  T20/T21/T22/T23 异常拒绝与 9 份 capture × 10 轮 lifecycle 全部通过，汇总
  `/tmp/batch23-24-final.log`。T22/T23 均标“自动验证通过，批末 UI 待验”；L3 条件未触发。
  最新 qrenderdoc 已重启并在同一进程打开与正式 capture SHA-256 相同的
  `/tmp/t22-batch-ui.rdc`，验证 Event range 1+2、EID 3/4 各自 Buffer 18/19、Mesh、
  ICB `Indirect argument` usage、红蓝输出、HTML 导出及 `No problems detected`。
  T22 Buffer CSV 保存确认前 Mac 再次锁屏；T23 尚未在本轮 UI 打开。第一项未完成：
  用户解锁后完成 T22 CSV 保存并在同一进程打开 `/tmp/t23-batch-ui.rdc`，完成 T23 全部
  L4，随后同步阶段/索引与下一批。临时两份 RDC 与正式文件哈希一致；全部 dirty 改动保留。

- 2026-09-24 T22 P23.4 批内转交检查点：T22 未注入 native、capture/XML、Replay API
  action/state/usage/readback/seek、逐份 CLI replay、三份顶点包原始字节、11 类异常拒绝、
  T20/T01/T13 定向与本场景 10 轮 lifecycle 通过；`PHASE23.md` 已标“批末 UI 待验”。
  样例 `util/test/demos/metal/metal_multi_command_icb.cpp`，正式 capture
  `captures/metal-smoke/t22_capture.rdc`。最终联合 L1/L2/CLI/lifecycle 与 T22/T23 同轮 L4
  尚待批末；L3 未触发。全部未提交改动保留；第一项未完成是 `PHASE24.md` P24.1 T23
  indexed ICB fixture/native。

- 2026-09-24 BATCH21-22 关闭检查点：T20/T21 联合 native/capture/XML/Replay API/逐份 CLI
  replay、T20 六类/T21 九类异常拒绝及 8 份 capture × 10 轮 lifecycle 通过，日志
  `/tmp/batch21-22-final.log`。最终 Viewer 标签/五字段格式修复后，最新 qrenderdoc 同一进程
  验收 T20/T21，并额外用 T13 确认非索引四字段路径；T20 的最终 IA/Mesh、ICB Resource
  与红色输出已再次复核。全部状态栏 `No problems detected`。`HANDOFF.md` 接手路径与
  `TEST_MATRIX.md` 批次说明已同步到 BATCH23-24，`git diff --check` 通过。
  L3 未触发。全部现有未提交改动必须保留。已新增 `BATCH23-24.md`、`PHASE23.md`、
  `PHASE24.md`；第一项未完成是 P23.1 T22 多命令 ICB fixture 未注入 native 验证。

- 2026-09-24 批末 UI 阻塞检查点：T20/T21 的最终联合自动验收已通过，日志和产物见顶部；
  第一项未完成是解锁 Mac 后用最新 `build-macos-debug/bin/qrenderdoc.app` 在同一轮依次验收
  `captures/metal-smoke/t20_capture.rdc` 与 `t21_capture.rdc` 的 Event/API、Pipeline、
  Mesh/Buffer/Resource、跳转/保存/export、输出与 `No problems detected`。若 L4 发现问题，
  修复后只重跑受影响自动项与 UI；之后同步阶段/索引文档并规划下一批。全部 dirty 改动保留，
  当前 L3 未触发，不得把阶段写成关闭。

- 2026-09-24 T21 P22.1 接手检查点：T20 自动验证通过、批末 UI 待验；T21 fixture 的原生
  5 帧及 driver 增量构建已通过。当前第一项未完成为 T21 capture/XML、CLI replay 和 Replay API
  smoke。此前全部未提交改动与 T20 capture 保留；联合 L1/L2 和双 capture L4 仍待批末执行。

- 2026-09-24 批次编排检查点：用户要求减少重复 QA，并要求新对话可批量执行 phase、批末统一
  验收。已新增 `BATCH21-22.md` 和 `PHASE22.md`，将 T20 单命令 ICB 与 T21 indexed indirect
  固定为两阶段一批；`PHASE21.md`、`HANDOFF.md`、`PLAN.md`、README、TEST_MATRIX 与 STATUS
  同步。T20/T21 尚未开发或验证，第一项未完成仍为 P21.1。每条切片的 native/capture/replay
  自动验证须立即完成；批末在最终构建上去重跑联合定向清单，qrenderdoc 同一轮验收两份
  capture，未通过 L4 不可标记任一阶段关闭。L3 默认不跑，触发后范围至 T00–T21。
  本次仅修改文档，全部既有未提交改动保留；最近代码证据仍是 T19 阶段关闭检查点。

- 2026-09-24 T19 阶段关闭检查点：P20.1-P20.4 已完成。新增 768-byte vertex storage fixture，
  slot 4/offset 256 used、slot 6/offset 320 unused；reflection/descriptor/`VS_Resource`、IA/storage
  分类、异常 slot/offset、标准 VS Storage Buffers 与 UI 导出均通过。必跑 T19/T18/T16 以及因
  `setVertexBuffer` 分类改动触发的 T02/T05 全部通过；T19 CLI 与 10 轮 lifecycle 通过，最终构建
  `/tmp/t19-final-build.log`。最新 qrenderdoc 已核对 Event/API、IA 空表、VS slot 4/6、标准
  Buffer/Resource、四象限和 `No problems detected`；UI HTML/CSV/bin 位于 `/tmp/t19_*`。
  定向结果没有跨场景未界定风险，故未触发 T00-T19 L3；最近完整基线仍是 T00-T18。
  全部既有未提交改动保留。第一项未完成为 `PHASE21.md` P21.1。

- 2026-09-24 验证清单细化：`HANDOFF.md` 和 `PLAN.md` 规定每份新阶段文档须列出必跑旧 T、
  条件触发旧 T、L4 检查和 L3 决策/完整范围。`PHASE20.md` 已明确 T19、必跑 T18/T16、
  条件触发 T02/T05；L3 默认不执行，触发时覆盖 T00-T19。T19 仍待从 P20.1 开始；
  本次只改文档，未运行代码测试，既有未提交改动保留。

- 2026-09-24 验证规则调整检查点：用户指出每个 Txx 阶段全量回归随场景数增长，要求小范围验证
  只覆盖本次修改。已同步 `HANDOFF.md`、`PLAN.md`、`PHASE20.md`：T19 从 P20.1 开始，先做
  T19 和实际受影响的 T18/T02/T05/T16 定向验证；T19 UI 用最新 qrenderdoc 做一次定向验收。
  仅在较大里程碑、发布/合并前或确有定向测试无法覆盖的跨场景风险时运行 L3。此前
  T00-T18 L3/L4 结果保留为历史证据，不重跑；全部未提交改动保留。本次只改文档，未运行代码测试。

- 2026-09-24 T18 阶段关闭检查点：P19.1-P19.4 完成。T00-T18 L3 日志
  `/tmp/t18-final-regression.log`（19×10 lifecycle，resident growth 1,556,480 bytes）；
  正式 capture、640-byte raw、UI CSV/HTML 产物位于 `captures/metal-smoke/`。最新构建 qrenderdoc
  用与正式 capture SHA-256 相同的 `/tmp/t18-ui-final.rdc` 完成 Event/API、FS Storage Buffers
  slot 3/offset 256/size 384、标准 Buffer Viewer、Resource Inspector `FS - Resource`、
  HTML/CSV 保存与 `No problems detected` 验收。UI CSV 前四行与自动 raw export 一致；
  UI 二进制菜单项未单独点击，raw 由自动 smoke 验证。T18 fixture、reflection/descriptor/usage、
  Metal FS Pipeline UI、usage 标签、自动 smoke 与阶段文档已修改；全部前序 dirty worktree 改动
  保留。第一项未完成为 `PHASE20.md` P20.1；不要重跑 T18 L3/L4。

- 2026-09-24 P19 L4 待验收检查点：T18 fixture、反射/descriptor/usage 和 FS Storage Buffers
  页面已实现；原生/capture/XML/replay、T04/T12/T10 定向均通过。T00-T18 完整 L3 日志
  `/tmp/t18-final-regression.log`：19×10 lifecycle、resident growth 737280 bytes，CLI replay 通过。
  完整回归无需重跑。qrenderdoc 旧进程打开 T18 后显示旧页面；已退出并启动最新进程，二进制含
  `Storage Buffers`，但 Mac 随即锁屏，Computer Use 报需用户解锁。L4 **未完成**，阶段尚未关闭。
  解锁后只需用最新 qrenderdoc 打开 `captures/metal-smoke/t18_capture.rdc`，核对 Event/API、
  EID 2 的 FS Storage Buffers slot 3/offset 256/size 384、未绑定 slot 5、标准 Buffer Viewer、
  Resource Inspector、HTML/raw save 和 `No problems detected`；然后同步阶段关闭文档。
  全部前序 dirty worktree 改动保留。最后成功命令为全量回归；没有待修复的代码失败。

- 2026-09-24 P19 L4 续接检查点：Mac 曾短暂解锁，最新 qrenderdoc 已显示 EID 2 的 FS
  Storage Buffers slot 3/offset 256/size 384、标准 Buffer Viewer 的四组 `float4` 和
  Resource Inspector 的 EID 2 usage。UI 发现 `PS_Resource` 文案误报 `FS - Texture`，已在
  `qrenderdoc/Code/QRDUtils.cpp` 为 Metal 改成 `FS - Resource`；修改后 T00-T18 全量回归
  `/tmp/t18-final-regression.log` 再次通过，19×10 lifecycle，resident growth 1556480 bytes。
  最新 qrenderdoc 可执行文件已重建并启动，但 Computer Use 随后确认 Mac 已锁屏且无法自动解锁。
  已请求用户手动解锁；第一项未完成
  是在新进程中核对更正后的 Resource Inspector 标签、HTML/raw save 与状态栏，**无需再跑 L3**。

- 2026-09-23 P19 开始检查点：已按最短接手路径读取 README、STATUS 当前阶段/最近检查点、PHASE19
  和 HANDOFF 验证规则；`git status` 确认前序未提交改动均保留，`git diff --check` 已通过。T18 尚未
  修改或验证。第一项未完成为 P19.1 原生 fixture；开发中仅跑 T18 与受影响 T04/T12，P19.4 才运行
  一次 T00-T18 L3 和一次最新 qrenderdoc L4。

- 2026-09-23 T17 阶段关闭检查点：P18.1-P18.4 全部通过。全量 L3 日志
  `/tmp/t17-final-regression.log`（18×10 lifecycle，resident growth 1,015,808 bytes）；正式 T17
  capture/XML/PPM/DDS、UI DDS/HTML 位于 `captures/metal-smoke/`。修改 T17 fixture、四个 render
  encoder 批量入口、replay fragment texture usage、自动 smoke/回归脚本和当前阶段文档；此前所有 dirty
  worktree 改动均须保留。最新 L4 已验证 Event/API、VS/FS Pipeline、空槽/未使用项、Mesh/Buffer/
  Texture/Resource、DDS/HTML 与状态栏。第一项未完成为 `PHASE19.md` P19.1；无需重跑 T17 L3/L4。

- 2026-09-23 T16 阶段关闭检查点：P17.1-P17.4 全部通过。全量 L3 日志
  `/tmp/t16-final-regression.log`（17×10 lifecycle，resident growth 114688 bytes）；正式
  T16 capture、XML、PPM、DDS 与 UI DDS/HTML 位于 `captures/metal-smoke/`。本轮修改 T16 fixture、
  render encoder bridge/wrapper/chunk、replay snapshot/descriptor/usage、Metal Pipeline VS UI、
  自动 smoke/脚本和文档；T00-T15 累计 dirty worktree 均不可回退。最新 L4 已验证 Event/API、
  VS Pipeline、Texture/Buffer/Mesh/Resource、DDS/HTML 与状态栏。第一项未完成为 `PHASE18.md`
  P18.1；不要重跑 T16 L3/L4，不要清理/回退工作区。

- 2026-09-23 T15 阶段关闭检查点：P16.1-P16.4 均已完成。T00-T15 L3 日志
  `/tmp/t15-final-regression.log`（16×10 lifecycle resident growth 737280 bytes）；正式 capture
  `captures/metal-smoke/t15_capture.rdc` SHA-256 为
  `208b794cc2ab49cf88f970cdda787bfb016f3483284b389130e05a39bae1a614`，自动 DDS
  `captures/metal-smoke/t15_output.dds` 480128 bytes。T01/T02/T05/T14 定向 smoke 均通过。
  非法 primitive 99 与零 vertexCount 的 `/tmp/t15-*.rdc` 派生 capture 均在 replay 拒绝。
- 本轮新增/修改：`util/test/demos/metal/metal_point_line.cpp`、demos CMake 列表、
  `metal_render_command_encoder.cpp`、`metal_replay.cpp`、`metal_replay_output_smoke.mm`、
  `test_metal_capture_macos.sh` 及阶段文档；既有 T00-T14 dirty worktree 均不可回退。
- 最新 L4：完全重启最新 qrenderdoc 后打开正式 T15，Event Browser 三个 action、EID 2/3/4
  的 Point List/Line List/Line Strip、API Inspector 的 Line Strip/6/3、标准 Buffer Viewer 的
  11 行顶点、Resource Inspector `Vertex Buffer` usage、三个 Mesh VS Input、Texture Viewer
  红点绿线蓝折线及 UI DDS/HTML export 均已验证，状态栏 `No problems detected`。
- 第一项未完成：进入 `PHASE17.md` P17.1；不要重跑 T15 L3/L4，不要清理/回退工作区。

- 历史检查点（T14 关闭时）：当时第一项未完成工作是 `PHASE16.md` P16.1 / T15 fixture。
- 工作区：T00-T14 累计未提交有效改动仍在。本轮新增 `metal_indexed_instancing.cpp`、indexed
  instanced-base render encoder capture/replay、精确 index binding/usage 和 T14 回归。
  不得清理、覆盖或回退；完整 dirty 列表以 `git status --short` 为准。
- 最近成功：`/tmp/t14-final-regression.log` 记录 T00-T14 native/capture/XML/output/data/state、
  15×10 lifecycle（resident growth 1,015,808 bytes）和逐份 CLI replay 全部通过。T02/T05/T13
  定向 output smoke 通过；offset 3/10 派生 RDC 均在 indexed-instanced chunk 拒绝。
- 最新 L4：完全重启最新 qrenderdoc 后，正式 `t14_capture.rdc` 的 Event Browser EID 2、API
  Buffer 18/offset 4、IA Pipeline Buffer 16/17/18、Resource Inspector `Index Buffer`、
  标准 Buffer Viewer 范围 4/6 与 `0/1/2`、Mesh instance 0/1、红蓝输出及状态栏
  `No problems detected` 均通过。`t14_indices_ui.csv` 内容为 0/1/2，
  `t14_pipeline_state_standard.html` 包含 Triangle List 与 Buffer 16/17/18。
- 当时的下一条安全操作：进入 `PHASE16.md` P16.1；T15 现已关闭，不要重跑 T14/T15 L3/L4，
  也不要清理累计 dirty worktree。

## Agent 工作节奏

2026-09-24 起采用更新后的 `PLAN.md` / `HANDOFF.md` 验证规则：一个 agent 默认连续负责完整 Txx
阶段，阶段内及收口只做当前与实际受影响旧路径的定向验证；涉及 UI 时完成一次最新 qrenderdoc
定向验收。L3 全量回归仅在较大里程碑、发布/合并前或确有定向测试无法覆盖的跨场景风险时执行。
阶段完成后可在当前对话继续；阶段中途只有先写好上述恢复检查点后才建议 compact。agent 不应
等待用户反复发送“继续”。

## 构建与启动

```sh
./util/buildscripts/scripts/build_metal_dev_macos.sh
./util/buildscripts/scripts/build_metal_dev_macos.sh --run
./util/buildscripts/scripts/test_metal_capture_macos.sh
```

- Build 目录：`build-macos-debug`
- App：`build-macos-debug/bin/qrenderdoc.app`
- 核心库：`build-macos-debug/lib/librenderdoc.dylib`
- App 内嵌核心库：`build-macos-debug/bin/qrenderdoc.app/Contents/lib/librenderdoc.dylib`；
  `build_metal_dev_macos.sh` 在增量构建后核对并同步，防止 UI 加载旧 replay 代码。
- CLI：`build-macos-debug/bin/renderdoccmd`
- Metal demos：`bin/demos_x64`
- smoke capture/XML：`captures/metal-smoke/`（生成目录，不提交）
- 清理：删除 `build-macos-debug` 后重新运行脚本；该目录已被 `.gitignore` 忽略。

## 最近验证

2026-09-23 T10 收口：`./util/buildscripts/scripts/test_metal_capture_macos.sh` 首次完整运行通过；
通用事件分组/UI 修正后按阶段门槛以最终代码复验，日志 `/tmp/t10-final-regression-post-ui.log`；
十一份 capture 各 10 次 lifecycle resident growth 1,277,952 字节。T10 自动断言 buffer
copy/fill 的 EID 2→3→2 数据往返、texture copy 四象限、
mip1/mip3、资源 usage、最终四条色带和 468-byte DDS，T03/T09 路径与 T00-T08 均通过。
`/tmp/t10-final-cli-replay-post-ui.log` 记录 T00-T10 逐份 `--loops 1` 成功。最新 qrenderdoc app
包内库与构建库逐字节一致。
正式 capture 的 `/tmp` SHA-256 相同副本在 Event Browser 显示 `Copy/Clear Pass #1`、EID 1-6
blit 操作；API Inspector 可跳到 Buffer 18 和 Texture 20，Resource Inspector usage 为
`Copy - Dest`/`Clear`/`Generate Mips`。标准 Buffer Viewer 的 EID 2/3、Texture Viewer 的四象限
与 mip3 拾取 `(0.52549,0.51765,0.34510,1.00)`、EID 8 四条色带均与自动结果一致。
`captures/metal-smoke/t10_mips_ui_final.dds` 与自动 `t10_mips.dds` 各 468 字节、`cmp` 相同；
状态栏 `No problems detected`。

2026-09-23 T09 收口：`./util/buildscripts/scripts/test_metal_capture_macos.sh` 最新运行通过，日志
`/tmp/t09-final-regression-v3.log`；十份 capture 各 10 次 lifecycle resident growth 为 475,136 字节。
T09 自动断言 12 子资源 readback/display、cube face pick、越界拒绝、12 条色带、fragment bindings 和
cube DDS；T03 texture 路径与 T00-T08 均通过。`renderdoccmd replay --loops 1` 对 T00-T09 逐一通过，
`git diff --check` 通过。最终 qrenderdoc 打开同一最新 T09 capture 的 `/tmp` 字节拷贝，EID 2 FS
绑定为 Texture 17/18/19 + Sampler 20；Texture Viewer 的 mip1/2、array slice1/2、cube X-/Z-
切换颜色正确，Z- 时“Save selected Texture”写出 512-byte
`captures/metal-smoke/t09_cube_ui.dds`，状态栏为 `No problems detected`。

```sh
cmake --build build-macos-debug --target renderdoc renderdoccmd build-qrenderdoc -j 12
./util/buildscripts/scripts/test_metal_capture_macos.sh
build-macos-debug/bin/renderdoccmd replay --loops 3 captures/metal-smoke/t00_capture.rdc
build-macos-debug/bin/renderdoccmd replay --loops 3 captures/metal-smoke/t01_capture.rdc
build-macos-debug/bin/renderdoccmd replay --loops 3 captures/metal-smoke/t02_capture.rdc
build-macos-debug/bin/renderdoccmd replay --loops 3 captures/metal-smoke/t03_capture.rdc
build-macos-debug/bin/renderdoccmd replay --loops 3 captures/metal-smoke/t04_capture.rdc
build-macos-debug/bin/renderdoccmd replay --loops 3 captures/metal-smoke/t05_capture.rdc
build-macos-debug/bin/renderdoccmd replay --loops 3 captures/metal-smoke/t06_capture.rdc
build-macos-debug/bin/renderdoccmd replay --loops 3 captures/metal-smoke/t07_capture.rdc
build-macos-debug/bin/renderdoccmd replay --loops 3 captures/metal-smoke/t08_capture.rdc
git diff --check
```

2026-09-22 T08 收口的最新一轮均通过。T08 自动回归检查 216-byte vertex buffer、4x MSAA descriptor、
显式 resolve/store action、三条 draw action、sample/resolve pipeline state、clear/draw/回退 resolve 图像，
最终 PPM 为左红、中绿、右蓝；完整 lifecycle resident growth 为 376,832 字节。qrenderdoc EID 4 的
OM 页显示 4x MSAA Color Target 与 1x Resolve Target；从 resolve 行进入 Texture Viewer 后中心拾取为
`(0.06275, 0.87451, 0.18824, 1.00)`，实际 HTML export 已核对，状态栏无错误。
较早的 T01 qrenderdoc 验证还覆盖 Texture Viewer 和窗口最大化 resize，画面正确，
状态栏显示 `No problems detected`，未再出现 degraded support 弹窗。事件回放自动化与 qrenderdoc
手工切换 EID 1 -> 2 -> 1 也通过，画面按 clear -> draw -> clear 正确变化。
`test_metal_capture_macos.sh` 还自动验证 capture texture 的紧密 BGRA 字节、clear/draw 像素拾取和
DDS 保存；`t01_texture.dds` 经 `file` 识别为 400x300、32-bit ARGB8888。
qrenderdoc 中也已手工验证 EID 2 的 Texture Viewer：右键拾取纹理坐标 `(202, 128)` 得到
`(0.61176, 0.34118, 0.32157, 1.00)`，状态栏为 `No problems detected`；“Save selected Texture”
成功写出 `t01_texture_ui.dds`，同样识别为 400x300、32-bit ARGB8888、480128 字节。
最终收口时将 `GetTextureData()`/`PickPixel()` 的资源类型校验改为 replay texture 描述表，避免因
live wrapper 不保留 capture record 而误拒绝合法纹理；修正后重新执行完整 capture smoke、T00/T01
单轮 CLI replay 和 `git diff --check`，均通过。
2026-09-21 的 Shader Viewer 收口中，自动回归验证了 `vs_main`/Vertex、`fs_main`/Fragment 和真实
MSL source。随后启动新构建 qrenderdoc，分别从 Function 13/14 打开 `captured.metal`，源码显示正确、
状态栏无错误。测试期间还复现并修复了持久化 D3D11 Pipeline State 子页面误用于 Metal capture 的
崩溃；修复后的同一路径已重新实机加载通过。
最终重新执行 `test_metal_capture_macos.sh`，生成新 T00/T01 capture，并通过 shader reflection、
texture data/pick/save 和 10 轮 event replay；随后 T00/T01 各自执行 `renderdoccmd replay --loops 3`
也均以状态 0 退出。
2026-09-21 的 Pipeline State 收口中，`renderdoc`、`renderdoccmd`、SWIG 与 qrenderdoc 均成功构建；
完整 smoke 重新生成 T00/T01，并自动验证 Metal pipeline/shader/topology/VB/color target 与原有
shader/texture/event 路径。随后关闭旧进程、启动新 qrenderdoc，在最新 T01 EID 2 的 Metal Pipeline
State 页面核对 `Pipeline State 15`、`Triangle List`、`Function 13/14`、`vs_main/fs_main`、
`Buffer 16`（0/96）和 `Texture 23`（mip/slice 0），状态栏为 `No problems detected`。
2026-09-21 的 M2.6 阶段关闭中，完整一键 smoke 再次通过并自动执行 T00/T01 各 10 次同进程
打开/关闭，warm-up 后 resident growth 为 491,520 字节；两份 capture 的 `renderdoccmd replay
--loops 3` 均以状态 0 退出。最新 qrenderdoc 同一进程完成 `T01 -> Close -> T00 -> Close -> T01`，
重开 T01 EID 2 后图像和 Pipeline State 仍正确，状态栏无错误；History/Debug 明确禁用。工作区路径
通过 Recent Captures 打开时曾被 macOS 26 阻塞在文件 `open()`，使用 SHA-256 相同的 `/private/tmp`
副本完成验证，因此该现象记录为宿主文件访问问题，不计为 replay 失败。
随后额外运行 T00/T01 各 50 次 lifecycle 压力检查，resident growth 为 1,441,792 字节并通过。
2026-09-21 的 T02 P3.1/P3.2 回归中，一键脚本重新构建并原生运行 T00/T01/T02，生成三份 capture
与 XML；T02 的 vertex descriptor、private depth texture/state、CCW/back cull、两组 viewport/scissor、
36 个 UInt16/UInt32 index draw 全部通过 structured 断言。Replay smoke 验证两个 indexed action、
真实 depth target、已知 index 字节与左右半屏输出；三份 capture 各 10 次生命周期循环通过，resident
growth 为 1,196,032 字节。
同一份 T02 capture 随后由最新 qrenderdoc 实机打开；EID 3 的 UInt32 indexed action、color/depth
outputs 和双立方体图像均正确，状态栏无错误。因 macOS 26 对工作区 Documents 路径弹出文件访问
授权，仍按既有方式使用 SHA-256 相同的 `/private/tmp` 副本完成验证。
2026-09-21 的 T02 P3.3 收口中，Metal pipeline snapshot 增加 vertex descriptor、index buffer、depth
与 raster 字段，并改为按 action event 保存 draw-time 状态。完整脚本重新截取三份 capture，验证
T02 clear/draw1/draw2/回退图像和两条 draw 的精确状态，lifecycle resident growth 为 720,896 字节。
随后关闭旧 qrenderdoc、启动最新构建，加载 SHA-256 与工作区一致的 T02 副本：EID 2 显示左半屏、
UInt16 Buffer 19/72，EID 3 显示右半屏、UInt32 Buffer 20/144；Float3/Float4、stride 28、less/write、
back/CCW 与 color/depth target 均正确，状态栏为 `No problems detected`。
2026-09-21 的 T02 P3.4/P3.5 收口中，Metal vertex inputs 接入通用 `PipeState`，Pipeline 资源激活
复用标准 Mesh/Buffer Viewer，并新增最小 VS Input wireframe renderer。完整脚本重新截取三份 capture，
验证 T02 generic vertex input 与 mesh output，lifecycle resident growth 为 294,912 字节。最新 qrenderdoc
重启后，EID 2 的 Mesh Viewer 显示 UInt16 展开的 `attr0/attr1` 和正确立方体线框；Buffer 18 自动显示
Float3/Float4 interleaved 数据，Buffer 19 显示 72-byte `ushort index`；EID 3 的 Buffer 20 显示
144-byte `uint index`，状态栏始终为 `No problems detected`。收尾时 T00/T01/T02 又分别执行
`renderdoccmd replay --loops 3`，并通过 `git diff --check`。

2026-09-21 的 T03 P4.1-P4.3/P4.5 验证中，新增 4x4 RGBA8 纹理四边形、`replaceRegion` upload、
一级 sampler resource、fragment texture/sampler replay 与通用 descriptor 查询。完整脚本重新构建并
生成 T00-T03 capture，T03 的 64-byte texel、四点 `PickPixel()`、四象限输出和 binding 均通过；
四份 capture 各 10 次 lifecycle resident growth 为 1,245,184 字节，随后四份 capture 的
`renderdoccmd replay --loops 3` 均以状态 0 退出。最终构建的 qrenderdoc 在 T03 EID 2 显示
Texture 17、Sampler 18、Float2/Float2、Triangle Strip 和正确四象限；标准 Texture Viewer 与
Resource Inspector 跳转均通过，状态栏为 `No problems detected`。阶段 4 下一项保持为 P4.4 布局收敛。

2026-09-22 的 P4.4 首个收敛切片中，Metal Pipeline 页面复用标准 Controls、`PipelineFlowChart` 和
隐藏 stage tabs，按 IA/VS/RS/FS/OM 重排现有真实状态。`Show Empty Items` 使用红色空槽，静态
binding reflection 尚未实现时 `Show Unused Items` 明确禁用；shader 行可直接进入 Shader Viewer。
完整 T00-T03 回归通过，四份 capture 各 10 次 lifecycle resident growth 为 294,912 字节。最终
qrenderdoc 先用 T03 验证 FS texture/sampler、empty index/depth 和 shader 跳转，再用 T02 验证 IA
UInt16 binding、RS viewport/scissor/raster 及 OM color/depth/depth-state，状态栏均为
`No problems detected`。该首个切片完成后，P4.4 转入资源树、上下文操作、预览/export 与 used
reflection 收敛。

2026-09-22 的 P4.4 第二个收敛切片将 Metal 表迁移到 `RDTreeWidget`/`RDHeaderView`，并把 Metal
ResourceId 接入通用 context/usage、thumbnail 和 preview 分发；资源双击路径保持不变。工具栏新增标准
Export 控件，T03 EID 2 实际导出的 HTML 含 IA/VS/RS/FS/OM、Triangle Strip、Texture 17 和
Sampler 18。完整 T00-T03 capture/replay 回归通过，四份 capture 各 10 次 lifecycle resident growth
为 524,288 字节；最终 qrenderdoc 保持运行且状态栏为 `No problems detected`。下一项转为 shader
resource binding reflection 与 used/unused 过滤。

2026-09-22 的 P4.4 第三个收敛切片让 replay 创建 pipeline 时请求 Metal argument reflection，并将
T03 的 `colourTexture`/`colourSampler` 映射到通用 shader reflection。fixture 将同一资源额外绑定到
未声明的 slot 1；自动测试验证 slot 0 为 used、slot 1 为 `NoShaderBinding + staticallyUnused`，且
used-only 查询只返回 slot 0。完整 T00-T03 capture/replay 回归通过，四份 capture 各 10 次 lifecycle
resident growth 为 540,672 字节。最终构建的 qrenderdoc 在 T03 EID 2 FS 页默认只显示 slot 0，勾选
`Show Unused Items` 后 texture/sampler 表各显示 slot 1，状态栏为 `No problems detected`。

2026-09-22 的 T04 P5.1-P5.4 中新增动态 uniform fixture，并补齐 `setFragmentBufferOffset` 的
capture/replay、fragment constant-block state/reflection/descriptor 与 FS Constant Buffers 表。完整
T00-T04 脚本通过，最终 PPM 左右像素为 `ff2010`/`10df30`，五份 capture 各 10 次 lifecycle resident
growth 为 376,832 字节。最终构建的 qrenderdoc 实机切换 EID 2/3，图像由左红/右背景变为左红/右绿，
binding 从 `Buffer 16 / 0 / 512` 变为 `Buffer 16 / 256 / 256`；双击进入标准 Buffer Viewer 的
offset 256、length 256 子范围，四个 float 原始值正确，状态栏为 `No problems detected`。

2026-09-22 的 T05 P6.1-P6.4 中新增两物理 vertex buffer 的 instanced fixture，并以
`instanceCount=3/baseInstance=1` 验证直接 draw、action/state、通用 VS input 和事件回放。完整脚本
通过，最终三处 PPM 像素为 `ff2010/10df30/1840ff`，六份 capture 各 10 次 lifecycle resident growth
为 573,440 字节。最终 qrenderdoc 的 EID 2 Texture Viewer 显示三色实例；IA 显示
`Buffer 16 / 24 / stride 8 / Vertex / 1` 与 `Buffer 17 / 96 / stride 24 / Instance / 1`；Mesh Viewer
instance 0/1 正确显示 baseInstance 后的 offset/colour，两个 Buffer Viewer 展示完整原始数组，状态栏
保持 `No problems detected`。

## 工作日志

| 日期 | 任务 | 结果 |
| --- | --- | --- |
| 2026-09-20 | M0.1 | 从官方 GitHub 检出 `v1.46`，成功 |
| 2026-09-20 | M0.2 | 创建 `metal-replay-v1.46` 分支，成功 |
| 2026-09-20 | M0.3 | 完成源码/工具链预检，发现 Qt 5 等依赖缺失 |
| 2026-09-20 | 计划初始化 | 建立 `docs/metal-replay/` 文档集 |
| 2026-09-20 | M0.4 | 通过 Homebrew 安装并核验 macOS/Qt 构建依赖 |
| 2026-09-20 | M0.5 | `ENABLE_METAL=OFF` 构建成功，qrenderdoc 主窗口启动成功 |
| 2026-09-20 | M0.6 | 修复 SDK 26 bridge 编译问题；`ENABLE_METAL=ON` 构建和启动成功 |
| 2026-09-20 | M0.7-M0.8 | 增加一键脚本并记录产物、启动和清理方法 |
| 2026-09-20 | M1.1-M1.2 | 增加 T00/T01 Metal fixtures；原生构建与运行成功 |
| 2026-09-20 | M1.3-M1.4 | 修复 macOS 26 drawable/residency 边界和主动 capture backbuffer；成功写出 `.rdc` |
| 2026-09-20 | M1.5-M1.6 | T00/T01 chunks、MSL、96-byte VB、draw/present 经 XML structured export 验证 |
| 2026-09-20 | M1.7 | 增加并通过 `test_metal_capture_macos.sh` 一键回归；阶段 1 完成 |
| 2026-09-20 | M2 预备 | 注册 Metal structured processor；真实 replay provider 待实现 |
| 2026-09-20 | M2.1 | 注册真实 Metal replay provider；T00/T01 可由 `renderdoccmd replay` 加载 |
| 2026-09-20 | M2.2 | 重建 T00/T01 基础 Metal 对象、资源和命令并在加载期间真实执行 |
| 2026-09-20 | M2.3 | 增加 render pass、clear、draw、present、capture end 的最小 action/event |
| 2026-09-20 | M2.4 开始 | qrenderdoc 成功加载 T01；确认 degraded 弹窗是输出未实现的主动能力标记 |
| 2026-09-20 | M2.4 output | 实现 Metal output/`RenderTexture()`/readback；T00/T01 像素回归和 qrenderdoc UI 验证通过 |
| 2026-09-20 | M2.4/M2.8 event replay | 保存 frame stream/event offset，实现 Full/WithoutDraw/OnlyDraw；自动和 UI 的 clear -> draw -> clear 验证通过 |
| 2026-09-20 | M2.5 texture data | 实现 capture texture readback/`PickPixel()`，自动验证 clear/draw BGRA 数据并成功保存 DDS |
| 2026-09-20 | M2.5 texture data 收口 | 修正 live texture 类型校验；重跑完整 capture smoke、T00/T01 CLI replay 和 DDS 格式检查，全部通过 |
| 2026-09-21 | M2.5 Shader Viewer 开始 | 开始汇合 source library MSL、function 入口和 Metal function stage；完成后需自动回归并启动 qrenderdoc 实机验收 |
| 2026-09-21 | M2.5 Shader Viewer 收口 | `ShaderEncoding::MSL`、source/entry/stage reflection、自动测试和 Function 13/14 实机查看通过；下一项为最小 Pipeline State |
| 2026-09-21 | M2.5 Pipeline State 开始 | 建立 Metal 专用最小 pipeline state，目标为 T01 draw 的 pipeline、vertex/fragment shader、vertex buffer、topology 和 color target |
| 2026-09-21 | M2.5 Pipeline State 收口 | Metal snapshot、通用 `PipeState`、proxy serialization、qrenderdoc 专用页、自动状态断言与最新 T01 EID 2 实机验证全部通过；M2.5 完成，转入 M2.6 |
| 2026-09-21 | M2.6 生命周期 | 修复 replay Metal 对象所有权和确定性 shutdown；T00/T01 各 10 次循环通过，resident growth 491,520 字节 |
| 2026-09-21 | M2.6 unsupported | shader debug 返回安全空 trace；histogram/pixel history/post-VS/custom/target shader 稳定降级并纳入自动回归 |
| 2026-09-21 | M2.6 UI/文档收口 | 最新 qrenderdoc 完成 T01/T00 关闭重开和禁用能力验证；新增 `PHASE3.md`，下一项为 T02 P3.1 |
| 2026-09-21 | P3.1 T02 fixture | 新增确定性 indexed cube、显式 vertex descriptor、UInt16/UInt32 index、Depth32Float 与固定 raster state；原生运行通过 |
| 2026-09-21 | P3.2 capture/replay | 新增 depth-state wrapper，接通 scissor/front-face/cull/直接 indexed draw，并修复 depth attachment 帧引用；structured capture 与 GPU replay 通过 |
| 2026-09-21 | P3.1-P3.2 自动回归 | T00/T01/T02 全量脚本通过；T02 index 字节/depth target/双视口图像和三份 capture 各 10 次 lifecycle 通过 |
| 2026-09-21 | P3.2 T02 UI | 最新 qrenderdoc 的 Event/Texture Viewer 显示 EID 2/3、UInt32 Buffer 20、FB0/DS 与双立方体，状态栏无错误；完整 Pipeline/Mesh 留给 P3.3/P3.4 |
| 2026-09-21 | P3.3 T02 state/event | 增加 draw-time event snapshot、vertex/index/depth/raster 状态和 clear/draw1/draw2/回退断言；全量回归及 qrenderdoc Pipeline State 实机验证通过，转入 P3.4 Mesh/Buffer UI |
| 2026-09-21 | P3.4-P3.5 T02 Mesh/UI 收口 | 通用 VS input、标准 Mesh/Buffer 跳转、Float3 indexed wireframe、自动输出断言及 qrenderdoc UInt16/UInt32 实机验证通过；阶段 3 完成，转入 T03 |
| 2026-09-21 | P4.1-P4.3 T03 texture/sampler | 完成确定性纹理四边形、upload/sampler/binding capture/replay、通用 descriptor 与标准资源跳转 |
| 2026-09-21 | P4.5 T03 自动化/UI | T00-T03 完整回归、四份 capture 三轮 CLI replay、生命周期及 qrenderdoc T03 实机验证通过；转入 P4.4 标准布局收敛 |
| 2026-09-22 | P4.4 标准阶段布局 | Metal Pipeline 接入 Controls + PipelineFlowChart + IA/VS/RS/FS/OM，empty-slot 与 shader 直接跳转通过 T02/T03 实机验证；继续资源树/反射收敛 |
| 2026-09-22 | P4.4 标准资源表/export | Metal 表迁移到 RDTree/RDHeader，接入通用资源操作/预览与五阶段 HTML export；T03 实际导出、完整回归和最终 qrenderdoc 验证通过；继续 shader reflection |
| 2026-09-22 | P4.4 shader reflection/过滤 | Metal argument reflection、slot 0/1 used-unused 自动断言、完整回归及 qrenderdoc 过滤实机验证通过；T03 阶段关闭，下一项为 T04 动态 uniform |
| 2026-09-22 | P5.1-P5.4 T04 动态 uniform | 512-byte uniform、fragment buffer offset、constant-block reflection/descriptor、事件回放、完整 T00-T04 回归与 qrenderdoc Buffer Viewer 实机验证通过；下一项为 T05 instancing |
| 2026-09-22 | P6.1-P6.4 T05 instanced mesh | 两个 vertex buffer、base instance、per-instance VS input、三色输出、完整 T00-T05 回归与 qrenderdoc Pipeline/Mesh/Buffer 实机验证通过；下一项为 T06 MRT + blending |
| 2026-09-22 | P7.1-P7.4 T06 MRT/blending | 双 color attachment、多输出 action、逐附件 blend/write mask、两张 texture 事件回放、标准 OM/Texture/export、完整 T00-T06 回归与 qrenderdoc 实机验证通过；下一项为 T07 depth/stencil |
| 2026-09-22 | P8.1-P8.4 T07 depth/stencil | combined attachment、front/back stencil、dynamic reference、五 draw 事件回放、标准 OM/Texture/export、共享阶段键盘导航、完整 T00-T07 回归与 qrenderdoc 实机验证通过；下一项为 T08 MSAA resolve |
| 2026-09-22 | P9.1-P9.4 T08 MSAA resolve | 4x MSAA attachment、显式 resolve、sample/resolve snapshot、三 draw 事件回放、标准 OM/Texture/export、完整 T00-T08 回归与 qrenderdoc 实机验证通过；下一项为 T09 mip/cube/array |
| 2026-09-23 | Agent 节奏重编排 | 固化一个 agent 完成一个 Txx 阶段、L0-L4 分层验证、阶段末单次完整回归/qrenderdoc、compact 安全检查点和下一任务提示模板；T09 仍待开始 |
| 2026-09-23 | P10.1-P10.4 T09 mip/cube/array | 12 子资源 fixture、slice-aware upload/readback/pick/display、通用绑定、标准 Texture Viewer 和 cube DDS 保存；完整 T00-T09 回归、CLI replay、lifecycle 与最终 qrenderdoc 验收通过；下一项 T10 blit |
| 2026-09-23 | P11.1-P11.4 T10 blit | buffer/texture copy、fill、mipgen 的 action/usage/seek/readback、标准 Buffer/Texture UI 与 DDS；完整 T00-T10 回归、CLI replay、11×10 lifecycle 和最终 qrenderdoc 验收通过；下一项 T11 compute |
| 2026-09-23 | P12.1-P12.4 T11 compute | 8×8 RGBA8 compute filter、dispatch action/usage/seek、读写 descriptor、CS Pipeline/Texture/Resource UI 与 DDS/HTML；完整 T00-T11 回归、逐份 CLI replay、12×10 lifecycle 和最新 qrenderdoc 验收通过；下一项 T12 argument buffer |
| 2026-09-23 | P13.1-P13.4 T12 argument buffer | 单层 fragment argument buffer、texture id(0)/sampler id(1)、capture/replay、间接 usage、通用 descriptor/reflection、标准 Viewer 跳转、DDS/HTML；T00-T12 L3、13×10 lifecycle 与最新 qrenderdoc L4 全部通过，下一项 T13 indirect draw |
| 2026-09-23 | P14.1-P14.4 T13 indirect draw | shared buffer offset 16 的 `3/2/1/1` 参数、indirect action/usage、标准 Buffer/Resource/Pipeline、raw `.bin`/HTML、异常 offset 拒绝；T00-T13 L3、14×10 lifecycle 与最新 qrenderdoc L4 均通过，下一项 T14 indexed instancing/base vertex |
| 2026-09-23 | P15.1-P15.4 T14 indexed instancing | UInt16 byte offset 4、baseVertex/baseInstance 1、双实例、精确 IA/Buffer/Mesh/usage/seek、异常 offset 拒绝；T00-T14 L3、15×10 lifecycle 与最新 qrenderdoc L4 均通过，下一项 T15 point/line |
| 2026-09-23 | P16.1-P16.4 T15 point/line | Point/Line/Line Strip 非零 vertexStart、三种 action/topology/Mesh VS Input、264-byte Buffer/usage/seek、非法参数拒绝、UI DDS/HTML；T00-T15 L3、16×10 lifecycle（resident growth 737280 bytes）与最新 qrenderdoc L4 均通过，下一项 T16 vertex texture/sampler |
| 2026-09-23 | P17.1-P17.4 T16 vertex texture/sampler | 四象限 native/replay、直接 vertex binding、VS reflection/descriptor/usage、标准 Viewer、非法 slot/资源拒绝、UI DDS/HTML；T00-T16 L3、17×10 lifecycle（resident growth 114688 bytes）与最新 qrenderdoc L4 均通过，下一项 T17 batch binding |
| 2026-09-23 | P18.1-P18.4 T17 texture/sampler batch binding | VS/FS 批量 range、空槽清除、used/unused、四象限、异常 RDC；T00-T17 L3、18×10 lifecycle（resident growth 1015808 bytes）与最新 qrenderdoc L4 均通过，下一项 T18 storage buffer |
| 2026-09-24 | P19.1-P19.4 T18 fragment storage buffer | slot 3/offset 256/size 384、reflection/descriptor/PS usage、FS Pipeline/Buffer/Resource、HTML/CSV/raw；T00-T18 L3、19×10 lifecycle（resident growth 1556480 bytes）与最新 qrenderdoc L4 均通过，下一项 T19 vertex storage buffer |
| 2026-09-24 | P20.1-P20.4 T19 vertex storage buffer | slot 4/offset 256 used、slot 6/offset 320 unused、IA/storage 分类、reflection/descriptor/VS usage、VS Pipeline/Buffer/Resource、HTML/CSV/bin；T19/T18/T16/T02/T05 定向、本场景 10× lifecycle 与最新 qrenderdoc L4 通过，L3 未触发，下一项 T20 ICB |
| 2026-09-24 | BATCH21-22 编排 | 固定 T20 ICB + T21 indexed indirect 两阶段；每条立即做功能自动验证，批末最终构建上去重跑联合定向/CLI/lifecycle，并同一轮 qrenderdoc 验收两份 capture；本次仅文档变更，第一项 P21.1 |
| 2026-09-24 | BATCH21-22 关闭 | T20 单命令 ICB、T21 indexed indirect 的联合自动、8×10 lifecycle 与同轮 qrenderdoc L4 通过；L3 未触发，下一批 BATCH23-24 |
| 2026-09-24 | BATCH23-24 关闭 | T22 多命令/non-zero range、T23 indexed ICB 的联合自动、9×10 lifecycle 与同轮 qrenderdoc L4 通过；L3 未触发，下一批 BATCH25-26 |
| 2026-09-24 | BATCH25-26 关闭 | T24 reset/reencode、T25 mixed draw/indexed ICB 的联合自动、11×10 lifecycle 与同轮 qrenderdoc L4 通过；L3 未触发，下一批 BATCH27-28 P27.1 |
| 2026-09-24 | BATCH27-28 关闭 | T26 pipeline inheritance、T27 buffers inheritance 的联合自动、11×10 lifecycle 与同轮 qrenderdoc L4 通过；L3 未触发，下一批 BATCH29-30 P29.1 |

| 2026-09-24 | BATCH29-30 自动完成，L4 待用户 | T28/T29 native/capture/XML/Replay API、10 类异常拒绝、联合 T11/T10/T01/T18/T19/T12/T16/T17、11×10 lifecycle 全部通过；合并 GUI 验收单 `QA_BATCH29-30.md`，T28/T29 均待人工 QA，批次未关闭 |
| 2026-09-24 | BATCH29-30 关闭 | T28/T29 的缩略图、T29 `$action()` 及先前 GUI 项由用户同轮确认；T28 DDS、T22/T25 新事件树复验通过。自动定向、逐份 CLI 与 lifecycle 通过，L3 未触发；下一批 BATCH31-32 P31.1 |
