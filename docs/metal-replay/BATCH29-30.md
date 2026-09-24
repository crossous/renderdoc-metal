# 批次 29–30：compute dispatch 与 buffer 绑定

本批固定包含 `PHASE29.md` / T28 `dispatchThreads` 和 `PHASE30.md` / T29 compute
buffer binding。先在现有 T11 texture filter 上增加非整除网格的 thread-level dispatch，
再验证 compute buffer 的读写、offset、资源状态和 Viewer。边界为 P29.1–P30.4；
跨 queue、heap、GPU 生成、compute ICB 不在本批。扩大影响范围前更新阶段及批次清单。

状态：T28/T29 自动验证与用户同轮 L4 均通过，批次已关闭。下一批为
`BATCH31-32.md`；T30/T31 尚未实施。

2026-09-24 最终 L4：用户确认 T28 右侧 Input/Output 缩略图、T29
`$action()` 筛选与右侧缩略图正常。此前 T28/T29 Event/API、Pipeline、
Buffer 20、逐事件画面、导出和 `No problems detected` 已确认，T28 GUI DDS
与自动参考相同；T22/T25 因事件树改动所需的最短复验也已确认。
最终定向及本次提交前 L3 结果见 `STATUS.md` 顶部；正式 captures 保留于
`captures/metal-smoke/`，未纳入 Git。

2026-09-24 缩略图修复检查点：用户重存的 T28 10×7 UI DDS 与自动参考逐字节一致，
并确认 T22 父行可展开为 EID 6/7 且绘制正常。Metal Headless output 修复后，
T01/T03/T09/T11/T12/T16/T17/T22/T25/T28/T29 的 Replay API 缩略图及原场景
定向、逐份 CLI replay 均通过；12 份 capture × 10 轮 lifecycle 通过，增长
1,376,256 bytes，`git diff --check` 通过。app 及正式 captures 路径不变，
当前内嵌 replay 库 SHA-256 为 `81738a1bcbde…`。L3 条件未触发。当前仅待
T28/T29 右侧 Input/Output 缩略图与 T29 `$action()` 筛选的用户最短 L4 复验；
此后已由用户确认，步骤和反馈见 `QA_BATCH29-30.md`。

2026-09-24 用户新反馈：T29 Buffer 20 与 API Inspector 的绑定调用已验收。
Metal 无筛选 Event Browser 原缺状态调用，ICB execute 父行原不包含子 draw、
点父行停在 draw 前，`0+1` 文字含糊，indexed instanced 子行原不显示实例数。
现已修复事件分配、ICB MultiAction 层级/父行 seek、范围与实例数。正式 T28/T29
captures 已更新，最新自动证据及旧场景范围见 `STATUS.md` 顶部；当前 GUI 最短
验收步骤见 `QA_BATCH29-30.md` 顶部。随后用户确认 T25 多实例显示、ICB 子 draw
收在 execute 内、点击父行画到最后子 draw，并确认 `setBuffer` 等状态 API 有 EID。
当时余项是 T28 10×7 UI DDS、T29 `$action()` 筛选、T22 指定父行红蓝画面；
批次未关闭。下一批 `BATCH31-32.md` 仅完成计划，尚未开始实现。

用户随后指出 Texture Viewer 右侧 Input/Output 有资源条目但纹理缩略图为空。
根因是 `ReplayOutput::DrawThumbnail` 使用 Headless output，而 Metal 的
`MakeOutputWindow` 只接受 macOS layer。此处属于当前 Viewer 质量缺陷，纳入
BATCH29-30 修复。新增 L2 定向编号：T01（普通 color output）、T03（fragment
input）、T09（mip/cube/array）、T11/T28/T29（compute input/RW output）、
T12（argument texture）、T16/T17（vertex/批量 texture）、T22/T25（ICB
事件 output）。验证 Headless thumbnail 尺寸、像素、事件切换及原有可见输出。
仅当定向范围暴露无法圈定的风险时升级 L3；用户 L4 需最短复验缩略图。

实现期间触及通用 descriptor access 映射与 Metal pipeline state 结构，故本批 L2 条件
T12/T16/T17 已触发，并列入最终必跑清单；未改 render pass 或资源初始内容/所有权，
T06/T07/T08/T00/T09 条件未触发。

## 执行顺序

1. T28 先证明未注入 native，再完成 capture/XML、Replay API/state/readback/seek 与异常拒绝；
   必要自动验证通过后标记批末 UI 待验，直接进入 T29。
2. T29 同样逐环节闭环；若改动 T28 或 T11 公共 compute 路径，重跑两者。
3. 最终构建上由 agent 按联合清单执行 L0/L1/L2、逐份 CLI replay、lifecycle，
   并核对可自动判定的导出内容。按 `QA_GUIDE.md` 给用户一份合并 T28/T29 的 GUI
   验收单；用户同轮验收两份正式 capture 并反馈后关闭批次。

## 最终联合验证清单

- L0：增量构建受影响 target、脚本语法检查、`git diff --check`。
- L1：T28/T29 各自 native、capture/XML、Replay API action/state/usage/readback/seek、
  非法 grid/binding/offset/resource 拒绝、CLI replay、本场景 lifecycle；核对 dispatch 前后
  和回退的原始数据、像素。
- L2 必跑一次：T11（原有 compute dispatch/texture）、T10（blit→render 可见性）、
  T01（最终 render/output 基线）、T18/T19（buffer 范围与 storage 分类）。
- L2 条件触发：若改动 shader reflection 或通用 descriptor，追加 T12/T16/T17；若改动
  render-pass/attachment，追加 T06/T07/T08；若改动资源初始内容/所有权，追加 T00/T09。
  其他影响先写明 T 编号和原因。
- 用户反馈触发的事件语义修复：Metal 帧内非 action 调用补 EID，ICB execute 作为可展开的
  MultiAction 并使父行定位到范围末端；范围改为明确的 location/length 文字，indexed
  instanced 子行显示实例数。此公共事件改动追加 T20/T21/T22/T23/T24/T25/T26/T27
  （ICB 层级、seek、state/output）与 T28/T29（compute 绑定和事件树）为必跑定向验证；
  另跑 T11（compute 基线）和 T01（render 基线）。旧 EID 断言、正式验收单须按新
  capture 更新，旧批次已验 L4 若受显示行为影响，登记最短复验。
- L4：用户按一次性验收单，在最新 qrenderdoc 同轮核对 T28/T29 的 Event/API、
  CS Pipeline、Texture/Buffer/Resource、dispatch 前后输出、跳转/保存/export 和
  `No problems detected`。agent 根据最终 capture 的 EID/资源编号写准确步骤与预期；
  收到用户通过反馈前，T28/T29 分别在 `QA_PENDING.md` 保持“待人工 L4”。漏看验收单
  或只反馈其中一项时，未验项持续待验；可继续后续功能，但本批不关闭，后续结果
  和交接都要提醒仍待人工 QA 的 T。
- L3 默认不跑。若联合清单暴露无法圈定的跨场景风险，或进入发布/合并门槛，记录原因后
  在最终代码上运行 T00–T29 全部 native、capture/XML、replay/output、逐份 CLI replay
  与 lifecycle；T29 尚未完成时范围到当时最新 T。

## 2026-09-24 自动验证与待验交接

最终脚本 `/tmp/run-metal-batch29-30.sh`、日志 `/tmp/batch29-30-final.log` 通过。
最终构建 `build-macos-debug/bin/qrenderdoc.app`，正式 capture 为
`captures/metal-smoke/t28_capture.rdc` 和 `t29_capture.rdc`。两条均完成 native 5 帧、
正式 capture 8 帧/XML、Replay API 的 action/state/usage/descriptor、前后/回退资源字节、
输出像素；T28 保存 408-byte DDS，T29 保存 336-byte raw。10 类异常 RDC 拒绝通过。
联合 T11/T10/T01/T18/T19/T12/T16/T17 定向 Replay API/output 与 CLI replay 通过；
11 份 capture × 10 轮 lifecycle resident growth 1,638,400 bytes。L0 增量构建、脚本语法、
Python 编译和 `git diff --check` 通过。随后仅调整 CS 页面空槽分类并重建最新
qrenderdoc，正式 T28/T29 Replay API smoke 再次通过。render pass 与资源所有权条件未触发；定向清单
未暴露无法圈定的跨场景风险，未到发布/合并门槛，L3 未触发。

用户合并 GUI 验收单见 `QA_BATCH29-30.md`。各 T 未明确确认的 L4 项继续留在
`QA_PENDING.md`；本批次不得标为关闭。

2026-09-24 用户已确认 T28 其余 GUI 项通过；EID 1 所见 Texture 30 是最终
backbuffer，需明确切到 Texture 20；已保存的 UI DDS 是 400×300 Texture 30，
需从 10×7 Texture 20 重存。首次 ⌘O 后重复弹出文件窗口已作快捷键时序修复并
增量构建，T28/T29 Replay API smoke 和 CLI 再通过，尚待用户实机复验。
T29 尚未反馈，详见 `QA_PENDING.md`；本批次继续未关闭。

随后用户找到 Texture List 并完成 T28 EID 1 Texture 20 检查，但确认第一次 ⌘O
修复无效，首份 T28 或 T29 加载后仍重复弹文件窗口。已撤回延迟动作，改为从自定义
全局快捷键表移除 Open Capture，仅保留 Qt QAction 的 ⌘O 注册；第二版 GUI 已构建，
SHA-256 `969a565fe559…`。用户须 ⌘Q 重开后确认。Texture 20 UI DDS 和 T29 L4
仍待验，本批次未关闭。

用户随后确认首次 ⌘O 重复弹窗已修复，并完成 T29 的 EID 4/5 Buffer 20 值
回退/前进、画面及 `No problems detected`。T29 HTML/CSV/Save Bytes 原始 336-byte
文件已生成，agent 核对 raw 与自动参考完全一致。用户发现 API Inspector 未显示
`setBuffer`/`setTexture` 状态调用；原组件只列 EID action，Metal 的这些调用无 EID。
已补列 action 前的 structured chunks，新 GUI 构建 SHA-256 `c1a773a05d56…`。
仅需用户 ⌘Q 重开后在 T29 EID 5 确认两条 `setBuffer` 可见，并在完整 Buffer 20
Viewer 确认 EID 2 全 `a5`；T28 的 10×7 UI DDS
仍待导出。两个阶段与批次继续未关闭。
