# Metal Replay 当前状态

本地 Mac 接手文字见 [2026-09-30 交接 prompt](LOCAL_MAC_AGENT_HANDOFF_2026-09-30.md)；
远程机停止 UE 大帧 GPU/GUI 回放。

## M9 / BATCH325：确认 23:15 WindowServer watchdog 重启，远程机继续停测

23:15:38 panic 明确为 WindowServer 连续 120 秒未 check in；其主线程
在 Metal/IOGPU/AGXG16X 提交路径，23:12 的快照里两个已终止的 UE 回放
探针仍列为 zombie，线程也停于 GPU 内核路径。此前 20:43 曾发生同类
watchdog，却没有这两个探针，所以本次与回放高度关联但不能判定唯一
根因或具体 Metal chunk。超时与杀进程不足以保证 GPU 内核工作结束。
本机停止 UE 大帧 GPU/GUI 回放；只读审计与编译可继续。见
[BATCH325](BATCH325_WINDOWSERVER_WATCHDOG_2026-09-29.md)。
**真帧未打开；无新定向 GPU 通过、全量回归或人工 UI 验收；累计成功
UI QA 增量 0。**

## M9 / BATCH324：UE 真帧停在 GPU 完成等待，本远程机停止大帧回放

用户提供的 API-only 日志未产生结果文件，不能判定成功或失败。助手在同一
旧库与 view 顺序诊断副本上做 12 秒和 18 秒两次有界 API 定位；后一次
栈采样落在 `FinishReplayCommands` → Metal `waitUntilCompleted`，物理
footprint 8.3 GiB，而脚本 RSS 仅采到 923.3 MiB。超时杀进程时系统拒绝
SIGKILL，进程处于 `?Es`；WindowServer 曾约 6.34 秒不就绪后恢复。
无新重启，但已达到本机停测门槛。XML 静态统计：12 heap/6.062 GiB、
64 次 commit、无编码 wait event；尚不能确定是 GPU 长任务还是驱动/回放
挂起。已修正两处扫描警告和脚本异常退出记录，终端库重建，**未用新库
再次跑 UE 真帧**。见 [BATCH324](BATCH324_UE58_GPU_WAIT_AND_REMOTE_HOST_STOP.md)。
**UE 真帧仍打不开；全量回归未跑；人工 UI 未验收；累计成功 UI QA 增量 0。**

## M9 / BATCH323：帧尾 buffer Empty 小夹具通过，UE 真帧待一次有界回放

在 D3D12/Vulkan 完成后回收与 UE 5.8 `DeferredDelete` 的横向对照后，
接通帧尾 buffer `Empty`：执行前拒绝后续引用，待已提交 GPU 工作完成后
执行，并在事件回跳前恢复。当前 Mac 的 4 KiB 原生 Validation、注入捕获、
API/CLI、GPU 字节/seek 及 1 例新负例通过；T62 的 10 例旧负例通过。
T62/T35 旧帧 API/CLI 通过，T319 专用 API/CLI 通过（通用 smoke helper
误用 T35 断言而失败）。原始 `UE58_capture.rdc` 的 15 个旧 view 顺序错误
仍在；已只读原件并生成 SHA 与 BATCH321 相同的诊断副本供有界验证。
旧真帧尚**未**用本批库做 GPU 回放，
也没有新人工 UI 验收；黑 RT 与 Shader Converter GPU VA 仍待核实。
见 [BATCH323](BATCH323_TERMINAL_PURGEABLE_REPLAY.md)。**仅定向终端通过；
全量回归未跑；人工 UI 通过 0，累计成功 UI QA 增量 0。**

## M9 / BATCH322：真帧 CPU 审计，GPU 完成边界仍未实现

在不执行 GPU 回放的条件下，`UE58_capture.rdc` 已可只读导出 XML：
11214 chunk、278 draw、89 render pass、10 个 MRT pass，确有 UE 场景与
Nanite scope。旧 `frame1770` 仍是单 RT 的 Slate 帧，不能解释真帧。
真帧的两个 buffer 经 command buffer blit 使用、提交后在 11174/11175
被置为 `Empty`，而捕获不含等待完成的 chunk；安全拒绝仍保留。原生
4 KB completion/Empty 探针仅编译，未在这台发生过 WindowServer watchdog
的远程 Mac 上运行。见 [BATCH322](BATCH322_UE58_CPU_AUDIT_AND_COMPLETION_PROBE.md)。
**仅 CPU 定向审计和编译通过；全量回归未运行；人工 UI 未验收；累计成功
UI QA 增量 0。**

## M9 / BATCH321：UE58_capture 原因定位；WindowServer watchdog 后暂停 GPU

用户 `UE58_capture.rdc`（SHA256 `a96e685f…`）确由受控 viewport 按钮保存，
但 15 个帧内创建的 buffer texture view 被错误写到父 placement buffer 之前。
捕获顺序代码已修，最小原生/注入/API/CLI/GPU 像素及事件回跳曾在重启前通过。
旧帧的诊断副本越过该处后，回放在帧内 `PurgeableStateEmpty` 触发 Metal
Validation 断言；随后本机发生 WindowServer watchdog 重启，因果未归属。
当前加了 GPU 执行前的安全拒绝，**旧帧仍不能正常开启**，等待在其他机器
验证完整 GPU 完成/资源回收功能族。重启后只完成无 GPU 的编译；定向回归、
全量回归、人工 UI 内容验收均未运行。详见 [BATCH321](BATCH321_UE58_CAPTURE_VIEW_ORDER_AND_WATCHDOG.md)。
累计成功 UI QA 增量 **0**。

> 新缺口的优先处理方法见 [跨 API 横向排查顺序](CROSS_API_TRIAGE.md)：
> 先查 RenderDoc 其他图形 API 与引擎源码，再处理 Metal 特有差异。

最后更新：2026-09-29（Asia/Shanghai）

## M8 / BATCH320：frame1770 人工 UI 内容失败，scope 已接通，黑屏仍在

用户已在 qrenderdoc 人工打开 `UE58_frame1770.rdc`：文件能打开，但无 UE
scope、唯一 RT 黑色，**内容验收失败**。XML 证实 UE 已发出 10 组
compute scope 和一个 `SlateUI` render scope；RDC 主体是 280 draw 的
Slate 合成帧，并非场景 GBuffer 帧。新 viewer 将 Metal debug group 映射到
事件树；旧 UE 帧终端显示 21 marker、280 draw。项目级按钮已按 UE 官方
RenderDoc 插件的受控 viewport 绘制顺序修改，UE 插件及库/viewer 构建通过，
但尚未点击新按钮验证真正场景截帧。旧帧整幅 RT 于首 draw 前后及帧尾均
全零；Shader Converter GPU VA 缺通用映射，尚未修复或证明唯一根因。
详见 [BATCH320](BATCH320.md)。**定向终端通过；全量回归未运行；人工
UI 已执行且内容失败；累计成功 UI QA 增量 0。**

## M7 / BATCH319：UE58 frame1770 定向终端打开通过，后续 UI 结果见 BATCH320

用户新截 v0x10 `UE58_frame1770.rdc`（29,633,881 字节，SHA256
`75dad9cecc5876c71db77de0e3a46b0f51371118e2d28ff3584113b148dda51d`），
注入库和插件 API 解析、保存均有日志；实际场景仍是 `Lvl_FirstPerson` PIE。
首次 API 打开通过，CLI 在帧内 Shared placement buffer `410638` 的帧首
重置失败后预览崩溃。按 D3D12/Vulkan 帧首资源边界修复后，同一帧一次
有界 Metal Validation API/CLI 均退出 0；T319 原生/注入、GPU 值、事件
回跳、两例负例，以及 T35/T59/T117/T313/T314/T317/T318 相关定向回归
通过（T318 使用专用探针）。最终库与 viewer 内嵌库 SHA256 均为
`c9bdc6bea56256140df635a65c30d5b56531c78a233b24a23568a5a372acfd6a`。
详见 [BATCH319](BATCH319.md)。**当批终端定向通过；全量回归未运行；
后续人工 UI 内容验收失败见 BATCH320，累计成功 UI QA 增量 0。**默认
bindless GPU VA 重定位及 Empty 最小场景仍待验证。

## M7 / BATCH315–318：BC placement 小夹具通过；UE v0xF 帧需重截

用户按钮生成 `UE58_frame5394.rdc`（SHA256 `fe304ecbd3392a4a4a35e45531e74e1b963bc266b558583ef5645d96c64f4081`），
确认 dyld 注入和截帧保存；缩略图是 `Lvl_FirstPerson`，不是 Empty。
一次有界 API 打开先拒绝 UE typed buffer view；定向修复后第二次拒绝
BC5 placement 纹理。BC1/BC1 sRGB/BC5 的原生 Metal Validation、
v0x10 帧首块快照、帧内拷贝、T317/T318 注入截帧、API/CLI、原始块
及 GPU 像素/事件回跳均通过；5 例负例安全拒绝，旧 T37/T117/T313/T59
定向通过。最终库和 viewer 内嵌库 SHA256 均为
`c13816cf6485ab945aa08ba476bc173613d5fbd609cb15518a6e65aabee49354`。
旧 v0xF UE 帧缺 BC 帧首初始内容，最终库有界 API 打开安全拒绝于 BC1
placement；不能继续用旧帧判断后续桥接。下一步用 v0x10 新库在 Empty、
非 Nanite 场景重截一次，再有界回放定位首个新阻塞。默认 bindless GPU VA
重定位仍未解决。详见 [BATCH315–318](BATCH315-318.md)。
**定向终端通过；全量回归和人工 UI 未运行；累计 UI QA 增量 0。**

## M7 新帧重截入口：前一阶段记录

本机 UE 5.8.3 项目与当时注入库预检通过；新增显式 `.rdc` 的有界 API/CLI
单次回放脚本，并以 T313 小帧验证其运行。磁盘上尚无采用 v0xF 新库的
UE 帧，旧 `frame4476` 不能验证 placement 修复。Shader Converter
bindless 表项是横向审计发现的后续风险，尚非新帧实测首阻塞。
18:26 新 UE 会话已确认 dyld 注入、插件 API 解析和引擎初始化；此前
`/tmp` 与 `/private/tmp` 路径字面比较导致的插件拒绝已通过启动脚本的
物理路径规范化修正。等待用户点击一次截帧。
见 [重截与回放入口](UE58_M7_RECAPTURE_GATE_2026-09-29.md)。
**全量回归和人工 UI 未运行；累计 UI QA 增量 0。**

## M7 / BATCH313–314：placement buffer 复用定向闭环，UE 旧帧仍拒绝

按 [跨 API 排查顺序](CROSS_API_TRIAGE.md)对照 D3D12/Vulkan 的独立资源身份与
UE `FreeBlock`，本机 Metal Validation 确认 placement buffer 原生 aliasability。
新 v0xF 截帧保留帧内 placement 创建顺序；T313 显式 alias、T314 释放复用的
原生/注入/API/CLI、GPU 字节及两轮事件回跳通过，受影响旧 T116/T117/T131/T132
定向回归及 39 例相关畸形输入通过。库和 viewer 内嵌库 SHA256 均为
`243043fb9dc1459afd3e1c2a6571dd7b880e1b76bf5b2122156a28a695fa1337`。
旧 v0xE `frame4476` 在最终库下 API/CLI 各单次仍安全拒绝于
`MTLHeap::newBuffer(offset)`；它缺帧内创建/释放时序，必须用新库重截。
详见 [BATCH313–314](BATCH313-314.md)。**没有全量回归或人工 UI 验收；
累计 UI QA 增量 0。**

## M6 / UE58 frame4476：首个回放阻塞为 placement heap 复用

用户完成默认 bindless 配置下的 UE 5.8.3 截帧后，编辑器已退出。
`UE58_frame4476.rdc` SHA256 `ab0e5a2918af5568a0257eb6f316140f68954f26d6c09e0b4f17606434b5bcbb`
（149,219,170 字节）。嵌入缩略图显示 `Lvl_FirstPerson` 编辑器视口，
不是 Empty 关卡。一次 Metal Validation API `OpenCapture` 在约 2 秒后
安全拒绝：placement heap `6047` 的 buffer `1374505` 与先前
`1373003` 占用区间重叠；无新 GPU Validation 报错。
详见[新帧首个阻塞证据](UE58_M6_FRAME4476_PLACEMENT_LIFETIME_2026-09-29.md)。
**此旧帧尚未打开**；本节首次诊断时 CLI 回放未运行，后续结果见上方 M7。
全量回归、人工 UI 验收均未运行，
累计 UI QA 增量 **0**。

## UE 5.8.3 `-BindlessOff` 启动隔离失败（16:22 session）

本机 `SocoTestProj` 的 `METAL_SM6` 编辑器在注入库
SHA256 `19d491f0…`、附加 `-BindlessOff` 后进入全局着色器编译，
未创建窗口。16:46 日志出现 3157 个全局着色器编译错误；
27 分钟时终止失败启动。没有新 `.rdc`，也没有回放或人工 UI 验收。
这证明该隔离命令在当前项目不可用，尚未单独证明编译失败归属 UE、
参数组合或注入。见[启动失败证据](UE58_M6_BINDLESSOFF_STARTUP_2026-09-29.md)。
去掉参数后的新 session `20260929-165339` 已由终端确认 UE 初始化和插件
解析 RenderDoc API；用户已截得 `UE58_frame4476.rdc`（149 MB，
SHA256 `ab0e5a29…`），缩略图是 `Lvl_FirstPerson` 编辑器视口。
截至记录时 UE 仍运行；新帧 GPU 回放尚未执行。
累计 UI QA 增量仍为 **0**。

## M5 / UE 5.8.3：Private 初始状态小夹具闭环，frame833 仍超时

本批接通 Private buffer 的帧首 GPU 上传和每次回放重置；原生/注入、
API/CLI 像素与回跳、未知资源负例及 T13/T21/T29 定向回归通过。
`frame833` 的 420 个 Private buffer（421,200,384 字节）上传成功，
但单次限时 75 秒 CLI 仍在 `ResourceId::503251` 等待超时；没有新
Validation 报错。UE SM6 bindless descriptor heap 中有捕获进程的
GPU VA，旧帧没有原地址映射，属于下一步待证明和处理的完整资源族。
详见[本批证据](UE58_M5_PRIVATE_INITIAL_BINDLESS_2026-09-29.md)。
**这张帧未打开**；全量回归和人工 UI 未运行，累计 UI QA 增量 **0**。

## M4 / UE 5.8.3：Private 间接绘制小夹具通过，frame833 仍未打开

现有 `UE58_frame833.rdc` 无需重截，viewer 已重建；本轮对 Private
普通/indexed 间接绘制及 buffer-only compute 的小夹具做了原生、注入、
API/CLI 定向验证，旧 T13/T21/T29 单次回归通过。UE 帧先越过原
`contents()` 断言及两个 compute 安全拒绝，但 14:21 的单次 CLI
60 秒超时，已终止；20 秒短 trace 进一步定位到等待
`ResourceId::503251`（XML 创建 chunk 3921、提交 chunk 10180）未完成。
暂停重复大帧回放，下一步静态审计其提交/依赖图并做小夹具。
库及 app 内嵌库 SHA256 `42ada08ec64c…`。
详见[本批证据](UE58_M4_PRIVATE_INDIRECT_2026-09-29.md)。
**新帧未通过**；全量回归及人工 UI 未运行，累计 UI QA 增量 **0**。

## M4 / UE 5.8.3 先前停点：frame833 回放仍阻塞，停止 GPU 复测

对 `UE58_frame833.rdc` 的定向诊断已越过 placement 纹理等早期拒绝，但
12:22 的 Metal Validation 在 `drawPrimitives(indirect)` 对 Private buffer
调用 `contents()` 时触发进程断言。已加入存储模式前置检查，同类 indexed
路径亦检查；未验证的 compute buffer dispatch 放行已撤回。viewer/CLI
重新编译通过，内嵌/CLI 回放库 SHA256 `319fd504dece…`；
**没有在新 GPU 错误后再运行回放**。因此本帧仍不可称为
可打开或画面正确；最后实测的阻塞是 GPU 可写、Private buffer 的
间接绘制参数与执行点元数据。撤回未验 compute 放行后，当前构建的
首个安全拒绝点尚未实测，可能更早。详见
[本批证据](UE58_M4_QRENDERDOC_3D_PLACEMENT_2026-09-29.md)。
当前构建没有全量回归或人工 UI 验收，累计 UI QA 增量 **0**。
请勿将当前 app 视为 `frame833` 的可用 viewer：Private 间接绘制现在会
明确拒绝，GPU 生成的参数仍需执行点读回和完整验证。

## M4 / UE 5.8.3 先前停点：新按钮截帧已保存，回放停在 3D placement 纹理

用户 `20260929-064750` 会话用 SHA256 `1ab4448a93b2…` 注入库成功保存
`UE58_frame833.rdc`（`472dfa48a56c…`，14,660,781 字节），说明上批
截帧结束的生命周期修复已在这次 UE 会话跨过原崩溃点。独立 worktree 的
`build-qrenderdoc/bin/qrenderdoc.app` 已编译，内嵌回放库与同次 CLI 构建均为
`8f79ed3345fd…`；**未启动 GUI**。该 CLI 在 Metal Validation 下单次打开
新帧退出 1，安全拒绝于 `MTLHeap::newTexture(offset)`：首个不支持的描述符
`MTLTextureType3D`（type 7），并非日志初看显示的资源选项或 hazard 值。
帧中共有 230 个 placement 纹理创建，含 2D、2D array、3D、cube；
后续格式、mip 和深度也超出现有 T117 的 2D 子集。需以完整功能族验证后
再扩展守卫。详见[本次构建与阻塞证据](UE58_M4_QRENDERDOC_3D_PLACEMENT_2026-09-29.md)。
**定向终端只确认了截帧保存和首个回放阻塞**；全量回归及人工 UI 未运行，
累计 UI QA 增量 **0**。
用户首次打开 viewer 于 11:50:01 在 `PythonContext::GlobalInit` 的
`PyDict_SetItemString` 闪退，第二次进程正常运行；同一栈在前一天也曾出现。
首次自动生成 Python stubs 写了版本标记却未生成文件，疑似 Python 3.14
兼容或失败后未清理错误状态；与新帧 Metal 回放阻塞是两个问题，详见上链证据。

## M4 / UE 5.8.3 先前停点：按钮截帧结束崩溃，生命周期修复待 UE 复测

用户 `20260929-024537` 会话确实在按钮请求后崩溃：RenderDoc
`EndFrameCapture` 向错误的原生对象发送 `waitUntilCompleted`，没有新 UE
`.rdc`；UE 随后在错误处理内卡住约两分钟。已按[跨 API 排查顺序](CROSS_API_TRIAGE.md)
核对并修复 command buffer record 跨 autorelease pool 的代理/原生保活。
本机原生及注入短 pool 夹具、按钮式 trigger/direct present、API/CLI 单次
回放通过；最终 dylib `1ab4448a93b2…`，详见[本批证据](UE58_M4_CAPTURE_LIFETIME_2026-09-29.md)。
当时尚未用新库复测 UE；后续 `20260929-064750` 会话已保存新帧，见上节。
全量回归和人工 UI 未跑；累计 UI QA 增量 **0**。

## M4 / UE 5.8.3：已有真帧定向终端回放通过

独立 worktree `renderdoc-metal-t312` 的 UE 真帧 `UE58_frame99.rdc` 现可在
Metal Validation 下单次 CLI 完整打开；API 对 165 个 draw 的首、四分之一、
中间、四分之三、末尾及回跳首事件检查 pipeline 身份通过。交错 command
buffer 的原生/注入截帧此前取得，本批 API GPU 数据/seek、CLI 和 2 例对象
身份负例通过；T35/T41/T56/T114 等旧帧定向回归通过。修复保留各 buffer
的 encoder 与状态，在捕获的 commit 点提交，并在 commit 前恢复 Shared CPU
写入；未活动的 Metal 反射参数不再强制要求 argument packet。细节见
[本批证据](UE58_M4_INTERLEAVED_REPLAY_2026-09-29.md)。

这是**定向终端通过**，尚无本批新 UE 截帧、原生 UE 画面/GPU 输出逐项对照、
全量压力回归或人工 UI 通过证据。当前项目亦未固定为严格非 Nanite 最小
场景；该里程碑仍待验证。累计 UI QA 增量 **0**，原 274 份待验不变。

## M4 / UE 5.8.3 先前停点：已截一帧，回放阻塞（历史）

2026-09-29 本机首次取得 UE 真帧 `UE58_frame99.rdc`（SHA256
`8a55f0e3738d8b10255dc855e13933ef6e9ab1055f7f59831fe3cfdba4a51022`）。
插件日志确认注入并保存。定向补齐 Shared Placement、4096 样本 counter buffer
和资源 `PurgeableStateEmpty` 后，单次 CLI 回放到达有效的**交错 command buffer**
时序：UE 先创建多个 buffer，再返回较早者编码。当前回放器在创建下一 buffer 时
提前提交上一 buffer，Metal Validation 于 `setCurrentCommandEncoder:` 触发
SIGABRT。现在各 encoder 入口及 commit 增加活动/未提交守卫；同一帧安全拒绝于
`MTLCommandBuffer::blitCommandEncoderWithDescriptor`，仍**不能打开 UE 帧**。
原生/注入的最小交错夹具重现该回放缺口；详见
[本机证据](UE58_M4_EVIDENCE_2026-09-29.md)。出现新 GPU Validation 错误后未再升级
UE 负载。全量回归、长时压力和人工 UI 均未运行；累计 UI QA 增量 **0**，
原 274 份待验不变。

## M4 / UE 5.8.3 先前接入检查点

见 [UE58_M4_INTEGRATION_2026-09-28.md](UE58_M4_INTEGRATION_2026-09-28.md)。
独立 worktree 从 `e0a26f7e6` 建立，保留旧目录 4 项改动。UE 5.8.3
项目级按钮插件已编译且注入库确实进入 UE。首次 blit pass 崩溃复现为
包装 counter buffer 传给原生 descriptor，以及跨 autorelease pool 的命令对象生命周期；
blit counter attachments 已接通为 Metal capture 版本 `0xD`。随后修复 compute
`useHeap`/`useHeaps` 包装对象被原生转发的问题，新 chunk 1384/1385。
本机两个功能族的原生 Validation、注入捕获、CLI/API 单次回放及 6+7 例
负例通过；T41/T101/T312 旧帧单次回放通过，详见批次记录。
最后一次 UE 启动 `20260929-001104` 已越过这两个崩溃点，随后在
`parallelRenderCommandEncoderWithDescriptor:` 的未接通守卫 SIGTRAP；
期间还有 AGX counter-buffer 类型错误及 heap identity 拒绝，须先定位。
**没有 UE `.rdc`，没有 UE API/CLI 回放、全量回归或人工 UI QA**；
累计 UI QA 增量 0，原 274 份待验不变。GPU 驱动调用链曾崩溃，停止继续增加 UE 负载。

## 当前安全检查点：BATCH311–312

公司 Codex API 临时接手请先读[2026-09-28 交接单](COMPANY_CODEX_API_HANDOFF_2026-09-28.md)；
本机暂停新增功能，等待额度重置后集中人工 QA。

- [BATCH311–312](BATCH311-312.md)：同步 tile/mesh pipeline 的 archive
  依赖与 archive-miss 选项在版本0xC接通。T311/T312 原生/捕获/API/CLI、
  本族315例畸形输入、32份跨族定向回归、T35/T305–T312各10次生命周期
  通过；旧版本截帧兼容。库/app `2f88058307f5…`，GUI待验**274份**；
  原始防御宏bridge/chunk **59/15**（含防御性拒绝与定义，不是未实现 API 数）。异步 tile/mesh archive 绑定仍拒绝；
  长时累计压力门禁与GUI未运行，旧panic未归因，本批无新增报告。

## 历史安全检查点：BATCH310

- [BATCH310](BATCH310.md)：Binary Archive mesh 函数添加接通为 chunk1383；
  T310原生/捕获/API/CLI与34例畸形输入、27份跨族定向回归、T35/T305–T310
  各10次生命周期通过。库/app `fecbaa45dd8a…`，GUI待验**272份**；
  原始防御宏bridge/chunk **57/15**。mesh pipeline archive 依赖尚未序列化；
  长时累计压力门禁与GUI未运行，旧panic未归因。

## 历史安全检查点：BATCH309

- [BATCH309](BATCH309.md)：Binary Archive tile 函数添加接通为 chunk1382；
  T309原生/捕获/API/CLI与25例畸形输入、22份跨族定向回归、T35/T305–T309
  各10次生命周期通过。库/app `13f127004ba0…`，GUI待验**271份**；
  原始防御宏bridge/chunk **58/15**。tile pipeline archive 依赖仍未序列化；
  mesh archive 变更仍拒绝。长时累计压力门禁与GUI未运行，旧panic未归因。

## 历史安全检查点：BATCH308

- [BATCH308](BATCH308.md)：Binary Archive 单节点 stitched library 添加
  接通为 chunk1381；T308原生/捕获/API/CLI与44例畸形输入通过，
  14份跨族定向回归、T35/T305–T308各10次生命周期通过。库/app
  `7c43ac13093f…`，GUI待验**270份**；原始防御宏bridge/chunk **59/15**。
  tile/mesh archive 变更与复杂 stitched graph 仍拒绝；长时累计压力门禁
  与GUI未运行。旧panic未归因，本批无新增报告。

## 历史安全检查点：BATCH307

- [BATCH307](BATCH307.md)：Binary Archive 的单个 visible 函数
  添加接通为 chunk1380；T307原生/捕获/API/CLI与43例畸形输入通过，
  13份跨族定向回归、T35/T305/T306/T307各10次生命周期通过。库/app
  `0a62ea89993e…`，GUI待验**269份**；原始防御宏bridge/chunk **60/15**。
  tile/mesh/library archive 变更仍拒绝；长时累计压力门禁与GUI未运行。
  旧panic未归因，本批无新增报告。

## 历史安全检查点：BATCH305–306

- [BATCH305–306](BATCH305-306.md)：空 Binary Archive + compute/render 函数
  变更两个新 chunk 1378/1379 闭环，同步/异步 pipeline 回放。修复旧版 capture
  条件字段误读；T301/T302 旧格式兼容。T305/T306 各30例畸形输入，T301/T302
  各19例旧格式畸形输入，12份跨族定向回归、T35/T305/T306各10次生命周期
  通过。库/app `1e0165e982e7…`，GUI待验**268份**。原始防御宏匹配
  bridge/chunk **61/15**；长时累计压力门禁与GUI未运行。旧panic未归因，
  本批无新增报告。

## 历史安全检查点：BATCH304

- [BATCH304](BATCH304.md)：异步 Stitched Library 新chunk1377闭环，
  T303/T304各17例畸形输入、12份跨族定向回归、T35+T303+T304各10次
  生命周期通过。库/app `a4547b2ebb75…`，GUI待验**266份**。
  原始防御宏匹配bridge/chunk **63/15**；复杂graph仍拒绝，旧panic未归因。
  长时累计压力回归与GUI未运行。

## 当前安全检查点：BATCH303

- [BATCH303](BATCH303.md)：单函数/单节点 Stitched Library 同步创建、
  函数表GPU调用及资源链已回放。17例畸形输入、11份跨族定向回归、
  T35+T303各10次生命周期打开通过。库/app `56bc82ca0211…`，GUI
  待验**265份**。bridge/chunk原始宏匹配**63/15**；异步 stitched
  后来在BATCH304接通，复杂graph仍明确拒绝。06:26 panic未归因，
  长时累计压力回归、UI未运行。

## 当前安全检查点：BATCH302

- [BATCH302](BATCH302.md)：同一 Binary Archive 的异步 compute/render
  pipeline 创建、依赖、GPU回放通过；19例畸形输入及T35+T302各10次生命
  周期通过。无新 bridge/chunk 数量变化；GUI待验**264份**。06:26 panic
  未归因，长时累计压力回归与 GUI 仍未执行。

## 当前安全检查点：BATCH301

- [BATCH301](BATCH301.md)：文件 URL Binary Archive 导入、payload 快照、
  compute/render pipeline archive 依赖与 archive-miss 选项已端到端接通。
  原生/捕获/API/CLI、原文件移走回放、19例畸形输入、8份跨族定向回归、
  T35+T301各10次生命周期通过。最终库/app `b297a988af63…`，GUI待验**263份**。
  旧chunk防御宏匹配 **17→16**；原 bridge 拒绝入口少1，但新增6个 archive
  变更方法显式拒绝，故原始 `METAL_NOT_HOOKED` 匹配 **59→64**，不可直接
  当成功能覆盖率。长时累计压力回归与 GUI 未执行，06:26 panic仍未归因。

## 当前安全检查点：BATCH299–300

- [BATCH299–300](BATCH299-300.md)：三/五实例复用两个不同 BLAS 的
  TLAS，新增可回放 chunk，保留旧截帧格式。两份原生/捕获/API/CLI、
  40例畸形输入、21份定向回归、2×10生命周期通过。库/app
  `ac97012d33b5…`，GUI待验**262份**；bridge/chunk宏匹配**59/17**。
  06:26 panic尚未归因，长时全量GPU回归及GUI未运行。下一阶段按
  [黑盒验收与稳定性门槛](BLACKBOX_GATE.md)转向崩溃隔离、M4/UE普通帧
  阻塞盘点，不再穷举光追描述符参数组合。
- [09-28稳定性复测](STABILITY_2026-09-28.md)：最终库对295份正常截帧
  分组API/CLI单次回放全过，全部295份分组各至少10次生命周期打开，
  另有3453例分组畸形输入按预期拒绝；未见新增panic。尚未跑完整累计
  脚本、295份×10同一进程或GUI，不归因旧panic。
- [Binary Archive资源链](BINARY_ARCHIVE_GATE.md)：T301已接通导入文件和两类
  pipeline；捕获期间修改 archive 的方法仍未支持。

## 历史安全检查点：BATCH298

- [BATCH298](BATCH298.md)：同一 BLAS 的 TLAS 实例上限由4扩到有界的
  65536；八实例原生/捕获/API/CLI、23例畸形输入、19份定向回归、
  10次生命周期打开通过。库/app `e67638f19213…`，GUI 待验 **260份**；
  bridge/chunk 原始宏匹配 **59/17**。仅八实例做 GPU 正例，不宣称
  65536实例已压测；长时全量 GPU 回归及 GUI 未运行。

## 历史安全检查点：BATCH294–297

- [BATCH294–297](BATCH294-297.md)：三/四个**不同** BLAS 构建 TLAS，
  补齐有序子资源数组、描述符快照、descriptor 分配与压缩复制路径。
  四份原生/捕获/API/CLI、72 例畸形输入、22 份去重定向回归、
  4×10 生命周期通过。库/app `bdc80ef81a6…`，GUI 待验 **259 份**；
  bridge/chunk 原始宏匹配 **59/17**。超过四个实例仍拒绝；长时全量
  GPU 回归及 GUI 未运行。系统只查到 06:26 的 `IOGPUResource` panic；
  快照含 `renderdoccmd`，panicked thread 属于 `kernel_task`/`IOGPUFamily`，
  测试可能是诱因，但没有足够证据判定根因，本批短测未复现。

## 历史安全检查点：BATCH290–293

- [BATCH290–293](BATCH290-293.md)：同一 BLAS 的 TLAS 从最多两个实例
  扩到三个/四个，含 descriptor 分配与压缩复制。四份原生/捕获/API/CLI、
  101 例畸形输入、29 份跨族哨兵、4×10 生命周期通过。库/app
  `986a0d98b8f8…`，GUI 待验 **255 份**；bridge/chunk 原始宏匹配
  **59/17**，但已有 bridge 守卫和 `buildInstances` chunk 的合法范围已
  实质扩大。超过四实例和两个以上不同 BLAS 仍明确拒绝；无新 panic，
  累计压力回归及 GUI 未执行。

## 历史安全检查点：BATCH284–289

- [BATCH284–289](BATCH284-289.md)：双三角形 indexed AS refit
  的 UInt16/UInt32、独立目标+压缩、换索引、换顶点及同时换两类资源。
  六份原生/捕获/API/CLI、300 例畸形输入、32 份跨族哨兵、6×10
  生命周期通过。库/app `2c2bc09e0e18…`，GUI 待验 **251 份**；
  bridge/chunk 原始宏匹配 **59/17**。无新 panic；累计压力回归和 GUI
  未执行。下一族优先选尚未接通的真实 bridge/chunk 资源链，
  不再仅以 indexed refit 参数组合扩大测试数；现有防御性拒绝不应删宏凑数。

## 历史安全检查点：BATCH281–283

- [BATCH281–283](BATCH281-283.md)：indexed refit 补足 UInt32
  indexOffset8、Float4 vertexOffset16 和两者加 descriptor 分配的组合。
  三份原生/捕获/API/CLI、150 例畸形输入、32 份跨族哨兵、3×10
  生命周期通过。库/app `2c2bc09e0e18…`，GUI 待验 **245 份**；
  bridge/chunk 原始宏匹配 **59/17**。无新 panic；累计压力回归和 GUI
  未执行。

## 历史安全检查点：BATCH272–280

- [BATCH272–280](BATCH272-280.md)：indexed triangle AS refit 扩展到
  Float4、noDuplicate、scratch/index/vertex 偏移、descriptor+vertexOffset、
  表槽1及 opaque 切换，再覆盖七参数组合。九份原生/捕获/API/CLI、
  450 例畸形输入、28 份哨兵、9×10 生命周期通过。库/app
  `2c2bc09e0e18…`，GUI 待验 **242 份**；bridge/chunk 原始宏匹配
  **59/17**。06:26 panic
  归因未明，本批短测无新 panic；累计压力回归和 GUI 未执行。

## 历史安全检查点：BATCH266–271

- [BATCH266–271](BATCH266-271.md)：indexed triangle AS refit 首批闭环，
  含 UInt16/UInt32、换 index buffer、独立目标、压缩复制和 descriptor
  分配。六份原生/捕获/API/CLI、300 例畸形输入、19 份哨兵、6×10
  生命周期通过。库/app `cf971d35cd45…`，GUI 待验 **233 份**；
  bridge/chunk 原始宏匹配 **59/17**。未测的 indexed refit 参数组合
  仍明确拒绝；06:26 panic 归因未明，累计压力回归和 GUI 未执行。

## 历史安全检查点：BATCH261–265

- [BATCH261–265](BATCH261-265.md)：修复 box/triangle refit 切换几何
  buffer 被错误拒绝，覆盖默认、格式化、noDuplicate 和独立目标。五份
  原生/捕获/API/CLI、155 例畸形输入、11 份哨兵、5×10 生命周期通过。
  库/app `c94ea0b15648…`，GUI 待验 **227 份**；bridge/chunk 原始
  宏匹配仍为 **57/17**。06:26 panic 与此前全量测试有时间关联但未能归因，
  本批短测未复现；累计压力回归和 GUI 未执行。

## 历史安全检查点：BATCH259–260

- [BATCH259–260](BATCH259-260.md)：refitted box AS 复制后射线以及复制
  目标再次 refit，GPU 射线 `1/1/0→0/1/0→0/1/0→1/1/0`。
  T259/T260 原生、捕获、API/CLI 通过，40/48 个畸形输入拒绝；九份
  定向哨兵和 T256–260 五份×10 生命周期通过。库/app `cbfb8ec0daff…`，
  GUI 待验 **222 份**。06:26 IOGPU panic 发生在此前全量测试期间，
  有时间相关性但未能归因；后续短测试未复现；
  累计压力回归和 GUI 仍未执行。

## 历史安全检查点：BATCH256–258

- [BATCH256–258](BATCH256-258.md)：bounding-box AS refit 已接通普通原位、
  组合偏移/表槽/禁止重复调用和独立目标。三份原生/捕获/API/CLI 的 GPU ray
  `1/1/0→0/1/0`，各 40 个畸形输入拒绝；5 份定向哨兵和 3×10 生命周期
  通过。库/app `cbfb8ec0daff…`，GUI 待验 **220 份**。bridge 原始调用
  **57 处**（新增长度为 2 的 box primitiveDataBuffer 显式拒绝）。06:26 IOGPU panic
  仍无法归因；后续短测试未出现新 panic，累计压力回归与 GUI 未执行。

## 历史安全检查点：BATCH251–255

- [BATCH251–255](BATCH251-255.md)：Float3/Float4 stride16 的 refittable
  triangle build/refit 已保真，覆盖原位、descriptor 分配加独立目标、
  禁止重复交点调用与 scratchOffset256，以及 refit 后压缩复制。五份
  原生/捕获/API/CLI ray `1→0`，各 34 个畸形输入安全拒绝；20 份
  哨兵和 5×10 生命周期打开通过。库/app `2668361301ba…`，GUI
  待验 **217 份**；原始宏匹配 **55 bridge 调用（另有定义 1）/
  17 未处理 chunk（含定义 1）**。仅有此前 06:26 一份 panic，因果
  未证实；全量压力回归与 GUI 未执行。

## 历史安全检查点：BATCH248–250

- [BATCH248–250](BATCH248-250.md)：indexed Float4/Float3 stride16
  接通 UInt16/UInt32、三偏移、descriptor 分配、禁止重复交点调用。
  三份原生/捕获/API/CLI ray `1,0,1`，各 25 个畸形输入拒绝；
  18 份跨族哨兵和 T244–250 的 7×10 生命周期打开通过。
  库/app `e9d06ac87d24…`，GUI 待验 **212 份**；原始宏匹配仍为
  **55 bridge 调用（另有定义 1）/17 未处理 chunk（含定义 1）**。
  只有此前 06:26 一份内核 panic，根因未证实；全量压力回归与 GUI
  未执行。

## 历史安全检查点：BATCH244–247

- [BATCH244–247](BATCH244-247.md)：静态非 indexed triangle 的
  Float3/stride16、Float4/stride16、vertexOffset16/scratchOffset256 及
  禁止重复交点调用已按新末尾 chunk 保真。四份原生/捕获/API/CLI ray
  `1,0,1`，各 20 个畸形输入安全拒绝，13 份跨族哨兵和 4×10 生命周期
  打开通过。库/app `21a036a49317…`，GUI 待验 **209 份**；原始宏匹配
  **55 bridge 调用（另有定义 1）/17 未处理 chunk（含定义 1）**。
  只有此前 06:26 一份内核 panic，因果未证实；全量压力回归和 GUI
  未执行。indexed/refit 的新顶点格式仍未接通。

## 历史安全检查点：BATCH240–243

- [BATCH240–243](BATCH240-243.md)：refittable triangle 的
  `allowDuplicateIntersectionFunctionInvocation=false` 已保真；原位、
  descriptor 分配加独立目标、scratchOffset256、refit 后压缩复制四份
  原生/捕获/API/CLI 均通过，GPU ray `1→0`。各 21 个畸形输入拒绝、
  最终库 21 份定向哨兵和 4×10 生命周期打开通过。错误 Encoder
  负例暴露并修复一次进程段错误；生命周期误报也已修正。库/app
  `61fe88a3591a…`，GUI 待验 **205 份**；**55 bridge 调用（另有定义 1）/
  17 未处理 chunk 宏匹配（含定义 1）**。未见新内核 panic，完整压力
  回归与 GUI 未执行。

## 历史安全检查点：BATCH237–239

- [BATCH237–239](BATCH237-239.md)：triangle 几何
  `allowDuplicateIntersectionFunctionInvocation=false` 已贯通分配、
  捕获与回放；非 indexed、UInt16 indexed、UInt32 indexed+三偏移/
  descriptor 三份均通过原生、API、CLI，各 16 个畸形输入拒绝、
  22 份跨族哨兵和 6×10 生命周期打开。库/app `103f584d33e5…`，
  GUI 待验 **201 份**；**55 bridge 调用（另有定义 1）/17 未处理
  chunk 宏匹配（含定义 1）**。06:26 panic 根因未知，未见新 panic；
  完整压力回归和 GUI 未执行。

## 历史安全检查点：BATCH234–236

- [BATCH234–236](BATCH234-236.md)：bounding-box 几何的
  `allowDuplicateIntersectionFunctionInvocation=false` 已贯通 descriptor 分配、
  build 捕获和回放；基本、box/scratch/table 偏移、opaque 三种组合的 GPU
  命中及交点函数计数均一致。三份各 22 个畸形输入拒绝、15 份旧捕获哨兵、
  3×10 生命周期打开通过。库/app 内嵌库 `0a4547ec4347…`，GUI 待验
  **198 份**；原始标记仍为 **55 bridge 调用（另有定义 1）/17 未处理
  chunk 宏匹配（含定义 1）**。06:26 panic 仍无法归因，未见新 panic；
  完整压力回归和 GUI 未执行。

## 历史安全检查点：BATCH231–233

- [BATCH231–233](BATCH231-233.md)：非 indexed、indexed 和 UInt32+
  三偏移/descriptor 分配的 triangle 表偏移1已保真；错槽原生对照
  render ray `0`，正例原生与回放为 `1`。三份各 17 个畸形输入拒绝、
  最终库 26 份跨族哨兵、8×10 生命周期打开通过。库/app 内嵌库
  `08d484d3b59f…`，GUI 待验 **195 份**；**55 bridge 调用（另有定义 1）/
  17 未处理 chunk 宏匹配（含定义 1）**。06:26 panic 根因未知，
  完整 GPU 压力回归与 GUI 未执行。

## 历史安全检查点：BATCH227–230

- [BATCH227–230](BATCH227-230.md)：indexed triangle 的顶点/索引/scratch
  三偏移、UInt16/UInt32、直接/descriptor 分配与单偏移分支已保真；
  四份原生/捕获/API/CLI 均为 compute ray `1/0/1`、render ray `1`。
  各 18 个畸形输入拒绝、30 份哨兵、9×10 生命周期打开通过。
  库/app 内嵌库 `96ad1c3c78a3…`，GUI 待验 **192 份**；
  **55 bridge 调用（另有定义 1）/17 未处理 chunk 宏匹配（含定义 1）**。
  06:26 panic 根因未知，完整 GPU 压力回归与 GUI 未执行。

## 历史安全检查点：BATCH226

- [BATCH226](BATCH226.md)：UInt32 indexed triangle 的 indexOffset8 与
  descriptor 分配组合通过，原生/回放 ray `1/0/1`、render ray `1`；
  15 个畸形输入安全拒绝、19 份哨兵、7×10 生命周期打开通过。
  库/app 内嵌库 `99941fffb20b…`，GUI 待验 **188 份**；
  **55 bridge 调用（另有定义 1）/17 未处理 chunk 宏匹配（含定义 1）**。
  06:26 panic 根因未知，完整 GPU 压力回归与 GUI 未执行。

## 历史安全检查点：BATCH225

- [BATCH225](BATCH225.md)：indexed triangle 的非零 indexBufferOffset
  已按独立末尾 chunk 保真；无效索引前缀+offset8 的 GPU ray `1/0/1`、
  render ray `1` 在原生和回放一致。15 个畸形输入拒绝、24 份跨族哨兵、
  6×10 生命周期打开通过。库/app 内嵌库 `99941fffb20b…`，GUI 待验
  **187 份**；**55 bridge 调用（另有定义 1）/17 未处理 chunk 宏匹配
  （含定义 1）**。06:26 panic 根因仍未知，完整 GPU 压力回归与 GUI 未执行。

## 历史安全检查点：BATCH223–224

- [BATCH223–224](BATCH223-224.md)：opaque box 几何及与 table/box/scratch
  偏移组合；两份原生/捕获/API/CLI 射线均 `0/0/0`，各 19 个畸形输入安全
  拒绝；21 份跨族哨兵、8×10 生命周期打开通过。库/app 内嵌库
  `1e37a4f471f9…`，GUI 待验 **186 份**；**55 bridge 调用（另有定义 1）/
  17 未处理 chunk 宏匹配（含定义 1）**。06:26 panic 根因仍未确定，
  完整 GPU 压力回归与 GUI/Computer Use 未执行。

## 历史安全检查点：BATCH221–222

- [BATCH221–222](BATCH221-222.md)：box 几何 intersection table offset1，
  再与 boxOffset48/scratchOffset256 组合，新增独立 chunk 保持旧捕获编号。
  两份原生/捕获/API/CLI、20/20 畸形输入、19 份跨族哨兵与 6×10
  生命周期打开通过；GPU 射线均 `1/1/0`。库/app SHA `44cc7a5efb3f…`，
  GUI 待验 **184 份**；**55 bridge 调用（另有定义 1）/17 未处理 chunk
  宏匹配（含定义 1）**。06:26 panic 根因未知，完整 GPU 压力回归暂停；
  无 GUI/Computer Use。

## 历史安全检查点：BATCH218–220

- [BATCH218–220](BATCH218-220.md)：compute 交点函数表创建与单表/range
  绑定已接通；stride32 双盒、boxOffset48/scratchOffset256 的 GPU 射线
  结果 `1/1/0` 由原生与 API 回放双重证实。三份捕获、18/18/20 畸形输入、
  17 份跨族哨兵与 4×10 生命周期打开通过。新 chunk 追加到枚举末尾，旧
  T137 捕获兼容已复验。库/app SHA `d26ccf06016a…`，GUI 待验 **182 份**；
  **55 bridge 调用（另有定义 1）/17 未处理 chunk 宏匹配（含定义 1）**。
  06:26 panic 根因未知，完整 GPU 压力回归暂停；无 GUI/Computer Use。

## 历史安全检查点：BATCH216–217

- [BATCH216–217](BATCH216-217.md)：URL 动态库及与普通动态库的混合
  依赖已按捕获内字节重建，原始文件删除后回放仍通过。两份原生/捕获/
  API/CLI、11/13 个畸形输入、27 份跨族哨兵及 3 份×10 生命周期打开
  通过；私有临时目录无残留。仅支持可等长迁移 install name 的二进制，
  不可迁移时明确拒绝。库/app SHA `8d98d33c39c0…`，GUI 待验
  **179 份**；当前 **55 bridge 调用（另有定义 1）/17 未处理 chunk
  宏匹配（含定义 1）**。06:26 panic 根因未知，完整 GPU 压力回归
  暂停；无 qrenderdoc/Computer Use。

## 历史安全检查点：BATCH213–215

- [BATCH213–215](BATCH213-215.md)：box AS 非默认 stride32 的单盒、双盒、
  双盒加 box/scratch 偏移已接通。三份原生/捕获/API/CLI、28/29/29 个
  畸形输入、24 份跨族哨兵及 8 份×10 生命周期打开通过；第二盒内部几何
  的射线命中仍待程序化交点验证。库/app SHA `cffc4b0d117…`，GUI
  待验 **177 份**；当前 **56 bridge 调用（另有定义 1）/18 未处理 chunk
  宏匹配（含定义 1）**。06:26 panic 根因未知，完整 GPU 压力回归暂停；
  无 qrenderdoc/Computer Use。

## 历史安全检查点：BATCH209–212

- [BATCH209–212](BATCH209-212.md)：box buffer offset48、scratch offset256、
  双偏移与偏移描述符分配已接通。四份原生/捕获/API/CLI、
  24/24/23/24 个畸形输入、21 份跨族哨兵及 5 份×10 生命周期打开通过。
  库/app SHA `920be51b84d8…`，GUI 待验 **174 份**；当前
  **56 bridge 调用（另有定义 1）/18 未处理 chunk 宏匹配（含定义 1）**。
  06:26 panic 根因未知，完整 GPU 压力回归暂停；无 qrenderdoc/Computer Use。

## 历史安全检查点：BATCH204–208

- [BATCH204–208](BATCH204-208.md)：refit scratch 偏移、独立目标、
  独立目标压缩后 ray 及紧凑 refit scratch 已接通。五份原生/捕获/
  API/CLI、33/33/33/52/34 个畸形输入、47 份跨族哨兵及 18 份×10
  生命周期打开通过。库/app SHA `56c3811f7405…`，GUI 待验 **170 份**；
  当前 **56 bridge 调用（另有定义 1）/18 未处理 chunk 宏匹配（含定义 1）**。
  06:26 panic 根因未知，完整 GPU 压力回归暂停；无 qrenderdoc/Computer Use。

## 历史安全检查点：BATCH202–203

- [BATCH202–203](BATCH202-203.md)：refit 单/双三角形 descriptor 分配后
  build→refit→压缩→ray 通过。原生/捕获/API/CLI、45/47 个畸形输入、
  32 份跨族哨兵及 13 份×10 生命周期打开通过；库/app SHA
  `a7c43e2e37fa…`，GUI 待验 **165 份**。当前 **56 bridge 调用（另有
  定义 1）/18 未处理 chunk 宏匹配（含定义 1）**。06:26 panic 根因未知，
  完整 GPU 压力回归暂停；无 qrenderdoc/Computer Use。

## 历史安全检查点：BATCH200–201

- [BATCH200–201](BATCH200-201.md)：单/双三角形 refit 后压缩复制的
  AS 目标继续用于 GPU ray。原生/捕获/API/CLI、45/47 个畸形输入、
  29 份跨族哨兵和 11 份×10 生命周期打开通过；库/app SHA
  `45b447817a59…`，GUI 待验 **163 份**。当前
  **56 bridge 调用（另有定义 1）/18 未处理 chunk 宏匹配（含定义 1）**。
  06:26 panic 根因未知，完整 GPU 压力回归暂停；无 qrenderdoc/Computer Use。

## 历史安全检查点：BATCH196–199

- [BATCH196–199](BATCH196-199.md)：TLAS 普通复制及单/双实例 TLAS 压缩复制
  后 ray 通过。原生/捕获/API/CLI、26/29/31/32 个畸形输入、36 份跨族
  哨兵和 8 份×10 生命周期打开通过；库/app SHA `a08d2768c106…`，
  GUI 待验 **161 份**。当前 **56 bridge 调用（另有定义 1）/18 未处理
  chunk 宏匹配（含定义 1）**；06:26 panic 根因未知，完整 GPU 压力回归
  暂停，无 qrenderdoc/Computer Use，`supportsRaytracing` 仍 false。

## 历史安全检查点：BATCH194–195

- [BATCH194–195](BATCH194-195.md)：indexed 与多三角形的压缩 BLAS
  目标继续用于 TLAS GPU ray。原生/捕获/API/CLI、32/30 个畸形输入、
  62 份跨族哨兵及 14 份×10 生命周期打开通过。库/app 内嵌 SHA
  `2e33d8225d00…`，GUI 待验 **157 份**。
- box 压缩目标的后续 ray 需程序化交点路径；TLAS/refit 压缩来源
  未开放。当前 **56 bridge 调用（另有定义 1）/18 未处理 chunk
  宏匹配（含定义 1）**。06:26 IOGPUFamily panic 根因未知，完整 GPU
  压力回归暂停；无 qrenderdoc/Computer Use，`supportsRaytracing` 仍 false。

## 历史安全检查点：BATCH192–193

- [BATCH192–193](BATCH192-193.md)：普通复制与跨 CB 压缩复制的 BLAS
  目标继续用于 TLAS GPU ray；同时修复全部 11 条 AS chunk 回放路径的活动
  encoder 身份校验。原生/捕获/API/CLI、25/29 个畸形输入、60 份跨族
  哨兵及 13 份×10 生命周期打开通过。库/app 内嵌 SHA
  `2e33d8225d00…`，GUI 待验 **155 份**。
- box/多三角形/indexed 压缩目标尚未逐类 ray 验证；TLAS/refit 压缩
  来源未开放。当前 **56 bridge 调用（另有定义 1）/18 未处理 chunk
  宏匹配（含定义 1）**。06:26 IOGPUFamily panic 根因未知，完整 GPU
  压力回归暂停；无 qrenderdoc/Computer Use，`supportsRaytracing` 仍 false。

## 历史安全检查点：BATCH189–191

- [BATCH189–191](BATCH189-191.md)：box、多非 indexed 三角形、indexed
  三角形 AS 的压缩复制已能按 GPU 读回容量执行。原生/捕获/API/CLI、
  32/32/33 个畸形输入、36 份跨族哨兵及 10 份×10 生命周期打开通过。
  库/app 内嵌 SHA `f8fa07f7fd24…`，GUI 待验 **153 份**。
- 压缩目标的 build 元数据尚未传播与 ray 绑定验证；TLAS/refit 来源未开放。
  当前 **56 bridge 调用（另有定义 1）/18 未处理 chunk 宏匹配（含定义 1）**。
  06:26 IOGPUFamily panic 根因未知，完整 GPU 压力回归暂停；无
  qrenderdoc/Computer Use，`supportsRaytracing` 仍 false。

## 历史安全检查点：BATCH186–188

- [BATCH186–188](BATCH186-188.md)：实例 descriptor 直接分配 TLAS，
  单实例、同BLAS双实例、异BLAS双实例均闭环。原生/捕获/API/CLI、
  20/22/23 个畸形输入、32 份跨族哨兵及 12 份×10 生命周期打开通过。
  后续追加 instance descriptor 的 heap size/align 查询，原生/注入通过，
  当前库 20 份跨族哨兵通过。库/app 内嵌 SHA `b9b786586f9d…`，
  GUI 待验 **150 份**。
- 当前 **56 bridge 调用（另有定义 1）/18 未处理 chunk 宏匹配（含定义 1）**；
  现有 chunk 子集扩大，计数不变。06:26 IOGPUFamily panic 根因未知，
  完整 GPU 压力回归暂停；无 qrenderdoc/Computer Use，
  `supportsRaytracing` 仍 false。

## 历史安全检查点：BATCH183–185

- [BATCH183–185](BATCH183-185.md)：计算/片元/顶点/tile 加速结构空绑定
  清空接通。T183–T185 的原生/捕获/API/CLI、每份 33 个畸形输入、25 份
  跨族哨兵及 9 份×10 生命周期打开通过。库/app 内嵌 SHA
  `54ed11b8cce6…`，GUI 待验 **147 份**。
- 当前 **56 bridge 调用（另有定义 1）/18 未处理 chunk 宏匹配（含定义 1）**；
  新能力复用已有 chunk，计数不变。06:26 IOGPUFamily panic 根因未知，
  完整 GPU 压力回归暂停。无 qrenderdoc/Computer Use，
  `supportsRaytracing` 仍 false。

## 历史安全检查点：BATCH182

- [BATCH182](BATCH182.md)：多 indexed triangle descriptor 分配按索引
  buffer 容量有界开放，T182 双三角形/六 UInt16 索引的原生/捕获/API/CLI、
  20 个畸形输入、39 份跨族哨兵及 15 份×10 生命周期打开通过。
  库/app 内嵌 SHA `1a8c4816af5a…`，GUI 待验 **144 份**。
- 当前仍 **56 bridge 调用（另有定义 1）/18 未处理 chunk 宏匹配（含定义 1）**。
  06:26 IOGPUFamily panic 根因未知；完整 GPU 压力回归暂停，无 qrenderdoc/
  Computer Use，`supportsRaytracing` 仍 false。

## 当前安全检查点：BATCH181

- [BATCH181](BATCH181.md)：非 indexed 静态三角形 descriptor 分配按 count/
  buffer 长度有界开放，T181 双三角形的原生/捕获/API/CLI、18 个畸形输入、
  38 份跨族哨兵及 14 份×10 生命周期打开通过。库/app 内嵌 SHA
  `44a338d15489…`，GUI 待验 **143 份**。
- 当前仍 **56 bridge 调用（另有定义 1）/18 未处理 chunk 宏匹配（含定义 1）**。
  06:26 IOGPUFamily panic 根因未知；完整 GPU 压力回归暂停，无 qrenderdoc/
  Computer Use，`supportsRaytracing` 仍 false。

## 当前安全检查点：BATCH180

- [BATCH180](BATCH180.md)：box descriptor 分配改为有界 count/buffer 长度
  校验，三 box 正向闭环通过。原生/捕获/API/CLI、18 个畸形输入、37 份
  跨族哨兵及 13 份×10 生命周期打开通过。库/app 内嵌 SHA
  `ba68ff520274…`，GUI 待验 **142 份**。
- 当前仍 **56 bridge 调用（另有定义 1）/18 未处理 chunk 宏匹配（含定义 1）**。
  06:26 IOGPUFamily panic 根因未知；完整 GPU 压力回归暂停，无 qrenderdoc/
  Computer Use，`supportsRaytracing` 仍 false。

## 当前安全检查点：BATCH179

- [BATCH179](BATCH179.md)：双 bounding-box descriptor 分配 AS 已接通，
  `boxCount=2` 的原生/捕获/API/CLI、18 个畸形输入、36 份跨族哨兵及
  12 份×10 生命周期打开通过。库/app 内嵌 SHA `cb8f903f05ff…`，GUI
  待验 **141 份**。当前仍 **56 bridge 调用（另有定义 1）/18 未处理 chunk
  宏匹配（含定义 1）**。
- 06:26 IOGPUFamily panic 根因未知；完整 GPU 压力回归暂停，无 qrenderdoc/
  Computer Use，`supportsRaytracing` 仍 false。

## 当前安全检查点：BATCH178

- [BATCH178](BATCH178.md)：单 box 几何 descriptor 分配 AS 现可捕获/回放，
  GPU compacted size1280。原生/捕获/API/CLI、18 个畸形输入、35 份跨族
  哨兵及 11 份×10 生命周期打开通过。库/app 内嵌 SHA `9e5c56446ded…`，
  GUI 待验 **140 份**。
- 当前 **56 bridge 宏调用（另有定义 1）/18 未处理 chunk 宏匹配（含定义 1）**；
  增加的 1 处为 box 描述符显式拒绝检查，并非功能回退。06:26 IOGPUFamily
  panic 根因未知；完整 GPU 压力回归暂停，无 qrenderdoc/Computer Use，
  `supportsRaytracing` 仍 false。

## 当前安全检查点：BATCH176–177

- [BATCH176–177](BATCH176-177.md)：同一 AS build 中 vertexOffset16 与
  scratchOffset256 组合已由 size/descriptor 两种分配验证。原生/捕获/API/CLI、
  31 个畸形输入、33 份跨族哨兵及 13 份×10 生命周期打开通过。库/app
  内嵌 SHA 仍 `1bb6f06a68b5…`，GUI 待验 **139 份**。
- 当前仍 **55 bridge 调用（另有定义 1）/18 未处理 chunk 宏匹配（含定义 1）**。
  06:26 IOGPUFamily panic 根因未知；完整 GPU 压力回归暂停，无 qrenderdoc/
  Computer Use，`supportsRaytracing` 仍 false。

## 当前安全检查点：BATCH175

- [BATCH175](BATCH175.md)：非 indexed 静态三角形 AS 的 scratch buffer
  offset256 已接通；原生/捕获/API/CLI、13 个畸形输入、31 份跨族哨兵及
  11 份×10 生命周期打开通过。库/app 内嵌 SHA `1bb6f06a68b5…`，GUI
  待验 **137 份**。当前仍 **55 bridge 调用（另有定义 1）/18 未处理 chunk
  宏匹配（含定义 1）**。
- 06:26 IOGPUFamily panic 根因未知；完整 GPU 压力回归暂停，无 qrenderdoc/
  Computer Use，`supportsRaytracing` 仍 false。

## 当前安全检查点：BATCH174

- [BATCH174](BATCH174.md)：非 indexed 单三角形 vertexOffset16 的 descriptor
  AS 分配已接通，原生/捕获/API/CLI、5+12 个畸形输入、30 份跨族哨兵及
  10 份×10 生命周期打开通过。库/app 内嵌 SHA `8a9f713e6364…`，GUI
  待验 **136 份**。当前仍 **55 bridge 调用（另有定义 1）/18 未处理 chunk
  宏匹配（含定义 1）**。
- 06:26 IOGPUFamily panic 根因未知；完整 GPU 压力回归暂停，无 qrenderdoc/
  Computer Use，`supportsRaytracing` 仍 false。

## 当前安全检查点：BATCH173

- [BATCH173](BATCH173.md)：非 indexed 静态三角形 AS 顶点 buffer 的 16 字节
  偏移现可捕获/回放，前缀干扰数据验证实际 GPU 读取位置。原生/捕获/API/CLI、
  12 个畸形输入、29 份跨族哨兵及 9 份×10 生命周期打开通过。库/app
  内嵌 SHA `b5c8dff22b37…`，GUI 待验 **135 份**。
- 当前仍 **55 bridge 调用（另有定义 1）/18 未处理 chunk 宏匹配（含定义 1）**。
  06:26 IOGPUFamily panic 根因未知；完整 GPU 压力回归暂停，无 qrenderdoc/
  Computer Use，`supportsRaytracing` 仍 false。

## 当前安全检查点：BATCH171–172

- [BATCH171–172](BATCH171-172.md)：UInt32 indexed opaque descriptor 分配与
  非 opaque 交点拒绝形成双侧对照，GPU 值 1/0、绿色/红色。原生/捕获/API/CLI、
  29 个畸形输入、28 份跨族哨兵及 9 份×10 生命周期打开通过。驱动库/app
  内嵌 SHA 仍 `73a801b87e7b…`，GUI 待验 **134 份**。
- 当前仍 **55 bridge 调用（另有定义 1）/18 未处理 chunk 宏匹配（含定义 1）**。
  06:26 IOGPUFamily panic 根因未知；完整 GPU 压力回归暂停，无 qrenderdoc/
  Computer Use，`supportsRaytracing` 仍 false。

## 当前安全检查点：BATCH170

- [BATCH170](BATCH170.md)：UInt16 indexed 非 opaque 几何经自定义交点函数拒绝，
  原生/捕获/API/CLI 为 Shared0、中央红色；与 T167/T169 indexed opaque 的
  Shared1、绿色构成同族对照。12 个畸形输入、26 份跨族哨兵及 7 份×10
  生命周期打开通过。库/app 内嵌 SHA 仍 `73a801b87e7b…`，GUI 待验 **132 份**。
- 当前仍 **55 bridge 调用（另有定义 1）/18 未处理 chunk 宏匹配（含定义 1）**。
  06:26 IOGPUFamily panic 根因未知；完整 GPU 压力回归暂停，无 qrenderdoc/
  Computer Use，`supportsRaytracing` 仍 false。

## 当前安全检查点：BATCH169

- [BATCH169](BATCH169.md)：indexed opaque 单三角形 descriptor 分配已接通，
  与独立 build chunk 组成 GPU 闭环。原生/捕获/API/CLI、5+12 个畸形输入、
  25 份跨族哨兵及 6 份×10 生命周期打开通过。当前 **55 bridge 调用
  （另有定义 1）/18 未处理 chunk 宏匹配（含定义 1）**；库/app 内嵌 SHA
  `73a801b87e7b…`，GUI 待验 **131 份**。
- 06:26 IOGPUFamily panic 的 panicked task 为 `kernel_task`，报告含
  renderdoccmd 进程快照，但具体诱因未证实；
  完整 GPU 压力回归继续暂停。无 qrenderdoc/Computer Use，`supportsRaytracing` 仍 false。

## 当前安全检查点：BATCH168

- [BATCH168](BATCH168.md)：UInt32 indexed opaque 三角形补齐 GPU 正向闭环；
  UInt16/UInt32 现在各有 capture/API/CLI 与 12 个畸形输入测试。24 份跨族哨兵
  API/CLI、5 份×10 生命周期打开通过。驱动库/app 内嵌 SHA 仍
  `9a7f700ace5c…`，GUI 待验 **130 份**；计数仍 **55 bridge 调用（另有定义 1）/
  18 未处理 chunk 宏匹配（含定义 1）**。
- 06:26 IOGPUFamily panic 根因仍未知；完整 GPU 压力回归继续暂停。
  无 qrenderdoc/Computer Use，`supportsRaytracing` 仍 false。

## 当前安全检查点：BATCH167

- [BATCH167](BATCH167.md)：indexed opaque 三角形 AS build 现可捕获/回放，
  新增独立 chunk 并保留旧 indexed 默认语义。原生与捕获 GPU 命中、
  12 个畸形输入、23 份跨族哨兵 API/CLI、4 份×10 生命周期打开通过。
  当前为 **55 处 bridge 调用（另有宏定义 1）/18 处未处理 chunk 宏匹配
  （含定义 1）**；库/app 内嵌 SHA 均 `9a7f700ace5c…`，GUI 待验 **129 份**。
- 06:26 IOGPUFamily panic 未复现，根因仍未知；完整 GPU 压力回归仍暂停。
  无 qrenderdoc/Computer Use，`supportsRaytracing` 仍 false。

## 当前安全检查点：BATCH166

- [BATCH166](BATCH166.md)：显式 opaque 三角形 AS descriptor 分配子集已接通，
  原生、捕获、API/CLI GPU 命中及 5+11 个畸形输入通过；22 份跨族哨兵及
  4 份×10 生命周期打开通过。当前为 **55 处 bridge 调用（另有宏定义 1）/
  18 处未处理 chunk 宏匹配（含定义 1）**；此前记录的 bridge 54 少算 1 处，
  并非功能回退。库/app 内嵌 SHA 均 `a0d4076aeb32…`，GUI 待验 **128 份**。
- 06:26 IOGPUFamily panic 根因仍未知，完整 GPU 压力回归仍暂停。
  无 GUI/Computer Use，`supportsRaytracing` 仍 false。

## 当前安全检查点：BATCH165

- [BATCH165](BATCH165.md)：AS三角形几何显式`opaque=true`现可捕获/回放，
  新增独立chunk并保留旧默认/nonopaque/refit语义。与T148自定义拒绝函数
  对照，T165 GPU结果由未命中0转为命中1；原生、注入捕获、API/CLI、11个
  畸形build输入、20份跨族哨兵及5份×10生命周期打开通过，resident growth
  1,556,480 bytes。原始计数仍**54 bridge / 18未处理chunk宏匹配**（含定义1），
  因为本批扩展旧bridge子集、新增chunk，不伪报标记清零。
  库/app内嵌SHA均`19ae3d8f9332…`，GUI待验**127份**。
- 06:26 IOGPUFamily panic没有复现，根因仍未知；完整GPU压力回归仍暂停。
  无GUI/Computer Use，`supportsRaytracing`仍false。

## 当前安全检查点：BATCH164

- [BATCH164](BATCH164.md)：带默认descriptor的AS encoder创建bridge接通，新增
  独立chunk；两次创建驱动BLAS/TLAS及compute射线`0,1`，原生/捕获/API/CLI
  通过，10个畸形descriptor/身份输入干净拒绝。19份跨族哨兵API/CLI、4份×10
  生命周期打开通过，resident growth 1,064,960 bytes。原始bridge **55→54**；
  原始未处理chunk宏匹配仍**18**（含宏定义1），本入口不是既有旧chunk。
  该批库/app内嵌SHA均`0ace9b0c4581…`，当时GUI待验**126份**。
- 06:26 IOGPUFamily panic根因仍未证实，完整GPU压力回归仍暂停；无GUI/Computer Use。

## 当前安全检查点：BATCH163

- [BATCH163](BATCH163.md)：IFT嵌套visible table的显式render `useResource`路径
  已在原生与回放中验证。负例发现旧residency回放缺encoder身份检查，错误ID会
  触发进程异常；render四种与compute两种residency入口已补类型/当前encoder
  校验，T163的8+6个负例干净拒绝。17份跨族哨兵API/CLI、3份×10生命周期
  打开通过，resident growth 622,592 bytes。标记仍**55 bridge / 18旧chunk**；
  该批库/app内嵌SHA均`917ed5804329…`，当时GUI待验**125份**。
- 没有再发生kernel panic，但其原因仍不能由目前证据确定。06:26后的完整GPU
  压力回归仍未启动；此处是定向安全检查点，不是全量验收。无GUI/Computer Use。

## 当前安全检查点：BATCH161–162

- [BATCH161–162](BATCH161-162.md)：IFT嵌套visible table单槽/range两个bridge已接通，
  新增两个chunk。原生与捕获回放中，visible函数读取IFT buffer参数的0/1值，
  GPU结果分别为未命中/命中；T161/T162 API/CLI及零值对照通过，6/10个畸形
  输入干净拒绝。13份相关capture定向API/CLI、7份×10生命周期打开通过，
  resident growth 2,605,056 bytes。原始bridge **57→55**，旧chunk **18**；
  该批库/app内嵌SHA均`1ad5d34e7b80…`，当时GUI待验**124份**。
- 没有重跑06:26 panic后的完整GPU压力回归，也没有GUI/Computer Use。
  当时IFT嵌套表的显式`useResource`路径尚未接通，本样本原生无需它；不能把本批
  视作完整ray tracing或任意资源residency支持。`supportsRaytracing`仍false。

## 当前安全检查点：BATCH159–160

- [BATCH159–160](BATCH159-160.md)：接通intersection function table的buffer单槽/数组
  两个bridge，新增两个chunk。GPU交点函数读取buffer，值1命中、值0拒绝；原生
  Metal Validation、注入捕获、T159/T160 API/CLI、零值反向对照均通过。T159/T160
  分别7/11个畸形绑定干净拒绝，相关11份capture定向API/CLI通过；5份capture×10
  生命周期打开通过，resident growth 1,064,960 bytes。原始bridge **59→57**，
  旧chunk **18**；该批库/app内嵌SHA均`d9498312a666…`，当时GUI待验**122份**。
- 旧T148–T153原生probe的`reject_triangle`缺少`instancing`签名，旧GPU值0不能
  严格证明交点函数被调用。已修正并重录六份capture，原生/注入捕获/定向API/CLI及
  T148/T153共42个表负例通过；此前的全量通过数字只适用于旧capture，不可移作新
  capture全量证据。T159实测带`instancing`函数可写入参数buffer，并用只读0/1
  对照证明真实调用。新T148–T153的人工项仍是红色/Shared0，但SHA已更新。
- 06:26 IOGPUFamily panic的根因仍未被证明。之后无第二次panic，但**未运行完整
  GPU压力回归**；仍只做分组/定向测试，无GUI/Computer Use。`supportsRaytracing`
  仍false，T61 ICB GPU执行范围问题保留在PLAN。

## 当前安全检查点：BATCH156–157 与 06:26 GPU kernel panic

- [BATCH158 T36兼容回归](BATCH158-T36-VALIDATION.md)：分组验证暴露T36延迟store
  action被回放提前定值、旧`textureBarrier`在当前设备Validation下被拒绝两处问题。
  已按实际setter时序修复；仅对pass内尚无GPU工作的旧barrier安全略过，之后
  明确拒绝。T36 API/CLI、begin→end seek、15个负例与26份跨族哨兵均通过；
  T35/T36/T43/T148/T156/T157共60次生命周期打开通过，resident growth 901,120 bytes。
  同一次会话另分组跑过T01–T147及T148–T153/T156–T157的定向API/CLI，
  但不是全部使用最新库，不可计为新版本全量通过；无第二次kernel panic。

- [BATCH156–157](BATCH156-157.md)：接通fragment intersection table的
  `setOpaqueTriangleIntersectionFunction`单槽与range两个bridge入口、新增两个独立chunk。
  原生空表GPU输出0，设置后T156/T157均输出1且中央绿色；Metal Validation、注入捕获、
  T156/T157/T153定向API/CLI、T156/T157分别16/18个表负例及各29个AS负例通过。
  原始bridge标记61→59，旧chunk仍18；`supportsRaytracing`仍false。
- 2026-09-28 06:26:38发生macOS kernel panic，Apple `IOGPUFamily`断言
  `IOGPUResource::free called for resource still owned by an IOGPUDevice`，panic快照有
  `renderdoccmd`。与全量回归时间相关，但无法由当前日志确定具体capture或把根因
  归于T156/T157；重启前日志在/tmp丢失。重启后全量第二次运行到T71时已主动停止，
  **不能声称本批全量通过**。在隔离前不要自动重启完整GPU压力回归或连续大量
  malformed replay；先做轻量定向与静态检查。没有GUI/Computer Use。集中UI待验120份。
- 针对“加载畸形capture提前返回、挂起命令尚未完成即析构资源”的可疑路径，
  已在初次加载失败时以及设备析构前结束/提交/等待剩余replay命令，并在新表桥接对超大单槽index
  做转换前拒绝。修补后T148–T153/T156–T157共8份定向API/CLI通过，两个编码中
  失败的负例在Metal Validation下通过；**这不是panic根因已被证明修复**。
- 此历史检查点的库/app内嵌库SHA均`e291aae57542…`；T156/T157 capture SHA
  `96ef002c3c03…`/`61dc07c81b38…`。下一步隔离panic的最小触发器并审核资源
  生命周期；若属系统驱动限制，记录并保留不运行高风险全量的门槛。

## 当前恢复检查点：BATCH148–153 Render Intersection Function Table

- [BATCH148–153](BATCH148-153.md)：接通render PSO创建intersection table及
  fragment/vertex/tile的单槽与range绑定共7个旧chunk，另新增表`setFunction`和
  显式非不透明三角形build两个独立chunk。六份T148–T153的自定义交点函数真实改变
  GPU ray-query结果：普通compute维持`1,0,1`，render三阶段均为未命中`0`；
  fragment/vertex中央红色，tile为dispatch写Shared。原生Metal Validation、
  注入捕获、定向API/CLI、每份29个AS负例及20/22个表负例通过。发现并修复
  默认非不透明三角形与旧refit路径的分支优先级，修复后另用临时新录T141确认
  native/capture/API/CLI兼容，未覆盖正式T141 capture。
- 最终库全量**152 captures、3337 malformed、1520 lifecycle opens**通过，
  resident growth **12,845,056 bytes**，日志`/tmp/metal-batch153-final.log`。
  库与app内嵌库SHA均`3fbc11f8a1e5…`，六份capture SHA见BATCH与集中QA表。
  GUI/Computer Use未运行，待人工累计**118份**；未提交/推送。
- 原始标记**61 bridge / 18旧chunk**。bridge从60升至61是因为新表wrapper对8个
  尚不支持的资源参数/opaque/curve变体明确拒绝，且本批消除了7个原有入口标记；
  不将新增拒绝伪报为功能完成。`supportsRaytracing`仍为false。下一族优先原生验证
  intersection table资源参数或其余旧chunk；T61 GPU执行点ICB
  range事件树限制仍在PLAN单独列明，未被本批解决。
- 后续原生能力探针：Apple M2 Pro在Metal Validation下报告
  `AtBlitBoundary=0`、`AtDrawBoundary=0`；因此blit/render
  `sampleCountersInBuffer`两个旧chunk未冒险接通，也没有T154/T155成功capture。
  已在`Metal_Counter_Stage`留下可重复环境开关，后续需支持该能力的GPU验证；
  本机下一族改评估其他可执行的intersection table变体或剩余旧chunk。

## 当前恢复检查点：BATCH146–147 Vertex/Tile阶段TLAS消费

- [BATCH146–147](BATCH146-147.md)：接通`setVertexAccelerationStructure`与
  `setTileAccelerationStructure`两个旧chunk。T146 vertex ray-query命中第二BLAS，
  Shared结果1、中央绿色；T147 tile kernel ray-query命中，Shared结果1。原生
  Metal Validation、注入捕获、T144–T147定向API/CLI、T146/T147各29个畸形
  输入通过；整批回归**146 captures、3037 malformed、1460 lifecycle opens**通过，
  resident growth **5,718,016 bytes**，日志`/tmp/metal-batch147-full.log`。库/app
  内嵌库SHA `2140c924a230…`，T146/T147 capture SHA `abeaf2f81566…` /
  `10073db853eb…`。
  GUI/Computer Use未运行，待人工累计112份；原始标记60 bridge/25旧chunk，
  `supportsRaytracing`仍false。下一段优先评估intersection function table的
  可执行链或更多实例映射，不能仅靠单槽AS绑定宣称光追已完整支持。

## 当前恢复检查点：BATCH145 Fragment阶段TLAS消费

- [BATCH145](BATCH145.md)：在T144的不同BLAS双实例TLAS上，接通旧chunk
  `setFragmentAccelerationStructure`。fragment shader真实ray-query命中第二BLAS，
  Shared输出1、中央像素绿色；注入回放API验证TLAS身份、slot0、事件seek
  `1→7→1`及像素。原生Metal Validation、T142–T145定向API/CLI、29个畸形
  输入通过。整批回归**144 captures、2979 malformed、1440 lifecycle opens**通过，
  resident growth **7,913,472 bytes**，日志`/tmp/metal-batch145-full.log`。
  库/app内嵌库 SHA `a624ff54d9b8…`，capture `78a3802c5057…`；GUI/Computer Use未运行。
  原始标记60 bridge/27旧chunk；`supportsRaytracing`仍false。下一段优先验证
  vertex/tile阶段AS消费或更多实例映射，先做原生GPU探针。

## 当前恢复检查点：BATCH144 两个不同BLAS的Top-Level实例

- [BATCH144](BATCH144.md)：T144为两个实例分别构建不同三角形BLAS，实例索引0/1，
  各平移x=-2/+2；三条GPU射线从x=-2/+2/+3返回`1,0,1`，可区分第二实例误指向
  第一个BLAS。原生Metal Validation、注入截帧、T142–T144定向API/CLI、24个
  T144畸形输入均通过；整批回归**143 captures、2950 malformed、1430 lifecycle
  opens**通过，resident growth **6,225,920 bytes**，日志`/tmp/metal-batch144-full.log`。
  库/app内嵌库 SHA `42099b733d9f…`，capture `3afa4d1f0c9a…`；未做GUI/Computer Use，
  集中清单新增至109份。原始标记60 bridge/28旧chunk；`supportsRaytracing`仍false。
  下一段评估更多实例数量与child映射、交叉函数表等完整GPU可观察链。

## 当前恢复检查点：BATCH142–143 Top-Level AS实例与GPU射线

- [BATCH142](BATCH142.md)、[BATCH143](BATCH143.md)：新增默认布局单实例及同一BLAS的
  双实例TLAS尺寸查询、真实build与独立chunk。T142平移x=+2，两个GPU ray返回
  miss/hit=`0,1`；T143双实例平移x=-2/+2，三条ray返回hit/miss/hit=`1,0,1`。
  注入回放均有API Shared输出与事件seek验证。T135–T143共9份定向API/CLI、
  T142/T143各21/23个畸形实例输入通过。T142当时完整基线141 captures、2903
  malformed、1410 lifecycle opens通过；当前最终一体化全量**142 captures、2926
  malformed、1420 lifecycle opens**通过，resident growth **7,012,352 bytes**，
  日志`/tmp/metal-batch143-full.log`。库/app内嵌库`c028455d3a89…`，T142/T143
  capture SHA `54d563c23e9b…` / `0a41991b814b…`；`git diff --check`通过。
  T70/T133最新原生/捕获及离线预期拒绝复验通过。GUI/Computer Use未运行，累计
  待人工108份；未提交/推送。原始标记**60 bridge / 28旧chunk**，比T141的
  59/28多一个实例descriptor显式fallback，不代表功能退回。
  `supportsRaytracing`仍false；下一步扩展不同child AS、更多实例及间接/motion、
  intersection table等可执行链，不能把一/两个同BLAS实例当作通用TLAS支持。

## 历史检查点：BATCH141 GPU可观察的底层AS原位Refit

- [BATCH141](BATCH141.md)：单无索引Float3三角形的refittable build、独立command buffer
  内GPU blit更新顶点、原位refit及compute单槽AS绑定已接通。真实ray-query在Metal Validation
  下build后命中1、refit后未命中0；注入回放API验证Shared输出首→末为`1,9`→`1,0`，
  末→首→末seek一致。T135–T141共7份定向API/CLI、28个T141畸形chunk通过。
  最终一体化全量**140 captures、2882 malformed、1400 lifecycle opens**通过，resident
  growth **0 bytes**，日志`/tmp/metal-batch141-final.log`。库/app内嵌库
  `9f1860586102…`，T141 capture `0ca68274fcea…`，`git diff --check`通过。
  GUI/Computer Use未运行，累计待人工106份；不提交/推送。
  原始标记**59 bridge / 28旧chunk**：新增refit描述符安全守卫使bridge文本计数从51
  增至59，不代表已接通的三个入口退回；不能用原始标记数替代可执行功能验证。
  `supportsRaytracing`仍false；下一步评估instance/top-level、render阶段AS绑定或更广
  refit形态的完整GPU可观察链，不把当前单三角形子集当作完整光追。

## 历史检查点：BATCH140 GPU尺寸约束的压缩AS Copy

- [BATCH140](BATCH140.md)：独立command buffer的静态三角形build→Shared GPU尺寸
  写回→1280-byte目标分配→压缩copy→目标尺寸写回，原生/注入Metal Validation、
  T140与T135–T139定向API/CLI、31个T140畸形捕获（含目标过小及GPU尺寸不一致）
  均通过；新增错误encoder负例后为32个。最终全量**139 captures、2854 malformed、
  1390 lifecycle opens**通过，resident growth 5,586,944 bytes，日志
  `/tmp/metal-batch140-final2.log`；库/app `2458b8caf470…`，UI未运行，
  累计待人工105份。
  原始标记51 bridge/28旧chunk，新增显式fallback使文本计数+1。T140需前序
  单无索引三角形build与GPU尺寸写回来自同一前序command buffer，且GPU尺寸可读；
  其它时序仍拒绝。T70/T133原生/捕获及离线预期拒绝复验通过；未提交/推送。
  下一功能族先评估AS refit的真实GPU变化与可观察验证，再选实例AS或shader消费链；
  不能仅凭压缩尺寸不变就声称refit几何正确。

## 历史检查点：BATCH139 等容量AS Copy

- [BATCH139](BATCH139.md)：两个1536-byte AS经真实GPU build→copy→目标压缩尺寸
  写回，原生/注入Metal Validation、14份定向API/CLI及T135–T139合计95个畸形AS
  输入通过。T139目标size并未缩至1280，`copyAndCompact`仍未实现。
  最终全量**138 captures、2822 malformed、1380 lifecycle opens**通过，resident
  growth 7,159,808 bytes，日志`/tmp/metal-batch139-full.log`。最新库/app
  `48c3c41062f2…`；原始标记50 bridge/28旧chunk，新增的显式资源类型fallback
  导致文本计数+1，不代表功能退步。T70/T133原生/捕获及离线预期拒绝复验通过。
  GUI未运行，累计待人工104份；未提交/推送。

## 历史检查点：BATCH136–138 AS几何扩展与描述符分配

- [BATCH136–137](BATCH136-137.md)、[BATCH138](BATCH138.md)：单静态三角形的
  UInt16/UInt32 indexed build、单 bounding-box build，及单无索引三角形 descriptor
  形式创建 AS，均形成真实 GPU 捕获回放链。T135–T138 原生/注入 Metal Validation、
  13份受影响捕获定向 API/CLI、73个畸形 AS 输入通过；最终全量为**137 captures、
  2800 malformed、1370 lifecycle opens**，resident growth 7,389,184 bytes，日志
  `/tmp/metal-batch138-full.log`。当时库/app `be071fb3fda6…`，GUI未运行，
  待人工103份；`supportsRaytracing`仍为false。原始标记49 bridge/28旧chunk：
  新增的描述符校验分支含显式 fallback，原始文本计数不能当作能力回退。
  T70/T133原生/捕获成功、离线预期拒绝也在最终库上复验通过；未提交/推送。

## 历史检查点：BATCH135 AS build与尺寸写回

- [BATCH135](BATCH135.md)：单个静态无索引三角形 AS 的 `newAccelerationStructureWithSize:`、
  命令 encoder、build/压缩尺寸写回/end 已形成真实 GPU 捕获回放链。T135 原生/注入
  size1536、GPU compacted1280；API/CLI、前后 seek、18个畸形输入及10份定向回归通过。
  其它AS形态仍显式拒绝，`supportsRaytracing`维持false。原始标记 **40 bridge /
  28旧chunk**：新增 AS encoder 对多种未支持描述符的显式 fallback，使 bridge
  文本计数上升，不能解读为功能退步或完整支持。当时库/app均为
  `55f87e6705d0…`，T135 capture `dff173e98bae…`；全量 **134 captures、2745
  畸形、1340 lifecycle** 通过，resident growth 5,554,176 bytes，日志
  `/tmp/metal-batch135-final.log`。T70/T133原生/捕获成功、离线预期拒绝再次通过。
  UI未运行，累计待人工100份（T134无独立UI项）；未提交/推送。

## 历史检查点：BATCH134 AS尺寸查询与BATCH131–132 Heap安全子集

- [BATCH134](BATCH134.md)：在光追完整资源链之前，先接通静态三角形/box 的
  `accelerationStructureSizesWithDescriptor:` 与
  `heapAccelerationStructureSizeAndAlignWithDescriptor:` 纯查询。原生/注入三种描述符
  数值一致、Metal Validation、T134捕获及6份定向API/CLI通过。其它描述符仍保留
  bridge fallback；原始标记仍为 **35/29**，`supportsRaytracing`仍为false。
  集中回归与UI未运行；无独立UI功能验收。

- [BATCH131–132](BATCH131-132.md)：buffer/texture `makeAliasable` bridge与旧chunk
  接通，限 heap 资源标记后不再使用、无后续重叠分配；原生 Metal Validation、
  捕获、CLI/API、6个畸形目标拒绝及旧 heap 定向通过。原始剩余 **35 bridge /
  29旧chunk**，T70另计36/30。全量 **132 captures、2727畸形、1320
  lifecycle** 通过，resident growth 5,521,408 bytes，日志
  `/tmp/metal-batch132-full.log`。库/app `d4f73865b86c…`；GUI/Computer Use
  未运行，待人工99份；未提交/推送。T133原生同offset重叠复用和捕获成功，离线
  明确拒绝，不计成功回放或UI待验；真正alias生命周期与事件seek仍须架构处理。

## 历史检查点：BATCH119–130 Visible Function Table 完整阶段链

- [BATCH119–120](BATCH119-120.md)、[BATCH121–123](BATCH121-123.md)、
  [BATCH124–125](BATCH124-125.md)、[BATCH126–130](BATCH126-130.md)：
  render fragment/vertex/tile 和 compute 的直接 visible function 链接、handle、
  table、单槽/范围绑定，以及 compute argument buffer 内的函数表编码已通过真实
  GPU输出、Metal Validation、捕获、CLI/API和畸形输入检验。Tile 同步及异步
  descriptor 均保留链接函数，schema v9 兼容旧捕获。原始剩余 **37 bridge /
  31旧chunk**，T70另计38/32。全量 **130 captures、2721畸形、1300
  lifecycle** 通过，resident growth 8,093,696 bytes，日志
  `/tmp/metal-batch130-full.log`。库/app `cf2a2ccc430e…`；GUI/Computer Use
  未运行，待人工97份；未提交/推送。Intersection function table、acceleration
  structure 和重叠 alias 生命周期仍未接通。

## 历史检查点：BATCH118 一层嵌套ArgumentEncoder

- [BATCH118](BATCH118.md)：接通`newArgumentEncoderForBufferAtIndex:` bridge/新chunk，
  父子encoder反射、只读内层texture/sampler、buffer指针尺寸与对齐、资源关系及GPU
  四象限采样均经原生Metal Validation、捕获、CLI/API、18个畸形输入验证。原始剩余
  **45 bridge / 39旧chunk**，T70另计46/40。全量 **118 captures、2625畸形、
  1180 lifecycle** 通过，resident growth 6,668,288 bytes，日志
  `/tmp/metal-batch118-full.log`。T118 capture SHA `e3fc631efe28…`，库/app
  `84df606e5379…`；GUI/Computer Use未运行，待人工85份；未提交/推送。
  Pipeline State公开结构暂不显示内层成员树，更深嵌套/数组/可写成员仍未支持。

## 历史检查点：BATCH116–117 Placement Heap 非重叠资源

- [BATCH116–117](BATCH116-117.md)：接通placement heap显式offset buffer/texture
  两个bridge和新chunk，以设备size/align校验偏移、边界及不重叠范围。GPU数据、三阶段
  texture画面和seek经原生Metal Validation及CLI/API验证；25个新增畸形输入拒绝。
  原始剩余 **46 bridge / 39旧chunk**，T70另计47/40。全量 **117 captures、
  2607畸形、1170 lifecycle** 通过，resident growth 5,603,328 bytes，日志
  `/tmp/metal-batch117-full.log`。T116/T117 capture SHA `9ab0c4160699…`/
  `3051cc3039d7…`，库/app `8dab14641eb8…`；GUI/Computer Use未运行，
  待人工84份；未提交/推送。重叠alias/makeAliasable仍明确不支持。

## 历史检查点：BATCH104–115 Dynamic library 链接及异步预加载

- [BATCH106–109](BATCH106-109.md)、[BATCH110–111](BATCH110-111.md)、
  [BATCH112–115](BATCH112-115.md)：计算/渲染（fragment 与 vertex）pipeline 的
  同步和异步 dynamic-library 预加载，以及异步 source-library 编译选项，均经原生
  Metal Validation、捕获、CLI/API 回放和畸形输入拒绝验证。原始剩余
  **48 bridge / 39旧chunk**，T70另计49/40。全量 **115 captures、2582畸形、
  1150 lifecycle** 通过，resident growth 7,094,272 bytes，日志
  `/tmp/metal-batch115-full.log`。库/app `5043305f5061…`；GUI/Computer Use 未运行，
  待人工82份；未提交/推送。URL 动态库导入仍不支持。

## 历史检查点：BATCH104–105 Dynamic library 链接执行

- [BATCH104–105](BATCH104-105.md)：接通`newDynamicLibrary:` bridge和旧chunk，
  source-library v6保存dynamic选项与依赖，回放私有installName路径并物化动态库。
  T104一依赖GPU输出3，T105两依赖GPU输出6；原生、capture、CLI/API、
  19+14畸形输入及旧捕获均通过。原始剩余 **48 bridge / 39旧chunk**，
  T70另计49/40。全量**105 captures、2502畸形、1050 lifecycle**通过，
  resident growth 3,932,160 bytes，日志`/tmp/metal-batch105-full.log`。
  T104/T105 capture SHA `22147d810218…`/`7520798eaddd…`，库/app
  `7d2400de2ed2…`。URL导入仍缺口；GUI/Computer Use未运行，待人工72份；
  未提交/推送。

## 历史检查点：BATCH103 Counter 子范围和偏移

- [BATCH103](BATCH103.md)：8槽counter sample indices2–5、resolve range(2,4)
  到目标buffer offset16，原生/捕获/CLI/API和22负例通过。原始剩余仍
  **49 bridge / 40旧chunk**，T70另计50/41。103 captures、2469畸形、
  1030 lifecycle全量通过，resident growth 622592 bytes；T103 capture
  `a38e39c760c4…`，库/app `30a8ca932d9f…`。GUI/Computer Use未运行，待人工
  70份；未提交/推送。全量日志`/tmp/metal-batch103-full.log`。

## 历史检查点：BATCH102 Reflection BufferBinding Argument Encoder

- [BATCH102](BATCH102.md)：接通`newArgumentEncoderWithBufferBinding` bridge/新chunk1316，
  从reflection重建简单只读texture2d/sampler布局并核对长度/对齐；T102左右两packet
  原生、capture、CLI/API、13个畸形输入拒绝及旧捕获定向通过。原始剩余
  **49 bridge / 40旧chunk**，T70另计50/41。102 captures、2447畸形、
  1020 lifecycle全量通过，resident growth 2375680 bytes；T102 capture
  `b9df0c466069…`，库/app `30a8ca932d9f…`。GUI/Computer Use未运行，待人工
  69份；未提交/推送。全量日志`/tmp/metal-batch102-full.log`。

## 历史检查点：BATCH101 阶段边界 Counter Sampling

- [BATCH101](BATCH101.md)：接通counter sample buffer创建与blit resolve两个旧chunk，
  render pass四阶段索引及资源身份使用v5 schema。原生probe与夹具验证四个非零递增
  timestamp；捕获/CLI/API/22个畸形输入拒绝和旧v1–v4定向通过。原始剩余
  **50 bridge / 40旧chunk**，T70另计51/41。101 captures、2434畸形、
  1010 lifecycle全量通过，resident growth 3538944 bytes；T101 capture
  `394b6a8f960b…`，库/app `77b615b94015…`。GUI/Computer Use未运行，待人工
  68份；未提交/推送。全量日志`/tmp/metal-batch101-full.log`。

## 历史检查点：BATCH100 异步Mesh + 双层Rate Map

- [BATCH100](BATCH100.md)：异步mesh pipeline调用时快照保存非默认mesh grid
  上限2，双层Rate Map第二层真实光栅和回拷已在原生/捕获/CLI/API验证；10个
  异步mesh加26个rate-map畸形输入拒绝。原始剩余仍**51 bridge / 41旧chunk**，
  T70另计52/42。100 captures、2412畸形、1000 lifecycle全量通过，resident
  growth 1949696 bytes；T100 capture `6997b8195353…`，库/app
  `d349808b9692…`。GUI/Computer Use未运行，待人工67份；未提交/推送。
  日志`/tmp/metal-batch100-full.log`。

## 历史检查点：BATCH99 双层Rate Map第二层真实光栅

- [BATCH99](BATCH99.md)：mesh primitive的`render_target_array_index`将两个
  threadgroup分别绘制到slice0/1，后者半水平速率物理宽208；原生、捕获、CLI/API
  slice1像素与回拷验证通过。mesh-only pipeline非默认网格上限2纳入v4 schema，
  v1/v2/v3兼容定向通过；19个mesh+26个rate-map畸形输入拒绝。原始剩余仍
  **51 bridge / 41旧chunk**，T70另计52/42。99 captures、2376畸形、990
  lifecycle全量通过，resident growth 3014656 bytes；T99 capture
  `161abbbbaeb2…`，库/app `d349808b9692…`。GUI/Computer Use未运行，当时待人工
  66份；未提交/推送。日志`/tmp/metal-batch99-full.log`。

## 历史检查点：BATCH98 双层Rate Map绑定数组渲染目标

- [BATCH98](BATCH98.md)：真实双层map绑定2-slice array render target，mesh绘制
  slice0并blit到drawable；原生Metal Validation、捕获、CLI/API与26个畸形输入
  拒绝通过。新增回放守卫要求map层数与render pass有效array length一致。
  原始剩余仍为**51 bridge / 41旧chunk**，T70另计52/42；slice1的真实光栅
  输出尚未验证。库/app `d5ddec74b502…`，T98 capture `1e6e084fd2fe…`。
  GUI/Computer Use未运行，当时待人工65份；未提交/推送。全量回归见
  `/tmp/metal-batch98-full.log`。

## 历史检查点：BATCH95–97 Rasterization rate map

- [BATCH95–97](BATCH95-97.md)：接通map创建旧chunk1030和`copyParameterDataToBuffer`
  bridge/新chunk1315，pass map引用；T96真实半水平速率在本机使物理宽度400→208，
  T97支持双层descriptor及参数数据，capture schema v3仍可读取v1/v2。原始剩余
  **51 bridge / 41旧chunk**；T70另计52/42。T95/T96/T97定向验证和各17/17/22
  个畸形输入拒绝通过；全量97 captures、2303畸形、970 lifecycle通过，resident
  growth 1589248 bytes，日志`/tmp/metal-batch97-full.log`。库/app
  `d93f1c816b54…`。GUI/Computer Use未运行，当时待人工64份；未提交/推送。
  双层array目标渲染尚未验证。

## 历史检查点：BATCH93–94 Object 输入网格

- [BATCH93–94](BATCH93-94.md)：修复object shader输入网格错误受mesh输出网格
  上限约束；T93两个object threadgroup、T94线程网格64×1×1原生及回放
  验证通过，联合16个畸形输入拒绝。原始剩余 **52 bridge / 42旧chunk**，
  T70另计53/43。捕获SHA `0b4476799c01…`/`28f1ccb2592a…`，库/app
  `3612c349a613…`；94 captures、2247畸形、940 lifecycle全量通过
  （growth 1327104 bytes，`/tmp/metal-batch94-full.log`）。GUI/Computer Use
  未运行，61份待人工；未提交/推送。

## 历史检查点：BATCH91–92 多线程 Object stage

- [BATCH91–92](BATCH91-92.md)：修复 direct mesh 两种 draw 对 object threadgroup
  必须为单线程的错误限制；T91/T92 各用 4 线程 object shader，经真实 payload
  绘制三角形，联合定向及16个畸形输入拒绝通过。原始剩余 **52 bridge / 42旧chunk**，
  T70另计53/43。捕获SHA `3ea72dbf14f6…`/`e18b6eb53bb5…`，库/app
  `dd543ab096a7…`；92 captures、2231畸形、920 lifecycle全量通过
  （growth 1884160 bytes，`/tmp/metal-batch92-full.log`）。GUI/Computer Use
  未运行，59份待人工；未提交/推送。

## 历史检查点：BATCH88–90 Mesh 间接绘制

- [BATCH88–90](BATCH88-90.md)：接通 mesh 间接 draw（chunk1314），Shared
  偏移16、GPU写 Private 参数、Object/Mesh 三种真实绘制路径；各11个畸形输入
  拒绝。原始剩余 **52 bridge / 42旧chunk**，T70另计53/43。捕获SHA
  T88 `81a4e8f91479…`、T89 `67f3eb8a6005…`、T90 `8ca736e3c39f…`；
  库/app `d74b63d4a264…`。定向及90 captures、2215畸形、900 lifecycle
  全量通过（growth 2424832 bytes），日志`/tmp/metal-batch90-full.log`。
  GUI/Computer Use未运行，57份待人工；
  未提交/推送。

## 历史检查点：BATCH87 异步 Object/Mesh pipeline

- [BATCH87](BATCH87.md)：异步object+mesh pipeline的独立调用时descriptor快照、
  回调包装与离线重建接通；8个畸形输入拒绝。原始剩余**53 bridge / 42旧chunk**，
  T70另计54/43。捕获`f6b5c2a71ac9…`、库/app`1e9b10c9196a…`；定向通过，
  全量日志`/tmp/metal-batch87-full.log`。GUI/Computer Use未运行，54份待人工；
  未提交/推送。全量87 captures、2182畸形、870 lifecycle通过，增长
  2015232 bytes。

## 历史检查点：BATCH86 Object threadgroup memory

- [BATCH86](BATCH86.md)：动态object threadgroup memory绑定接通，真实object
  shader以scratch驱动payload，16/32/48-byte三阶段与5个畸形输入验证通过。
  原始剩余**53 bridge / 42旧chunk**，T70另计54/43。捕获`6b70534fc597…`、
  库/app`fbc12b54dfb3…`；86 captures、2174畸形、860 lifecycle全量通过，
  growth 1933312 bytes，日志`/tmp/metal-batch86-full.log`。
  GUI/Computer Use未运行，53份待人工；未提交/推送。

## 历史检查点：BATCH85 Object texture/sampler 绑定

- [BATCH85](BATCH85.md)：六种object texture/sampler绑定接通，真实采样经payload
  改变三次mesh三角形位置，28个畸形输入拒绝。原始剩余**54 bridge / 42旧chunk**，
  T70另计55/43。捕获`e3bf1eee74e6…`、库/app`6091de8a8265…`；85 captures、
  2169畸形、850 lifecycle全量通过，growth 3096576 bytes，日志
  `/tmp/metal-batch85-full.log`。GUI/Computer Use未运行，52份待人工；
  未提交/推送。

## 历史检查点：BATCH84 Object buffer/bytes 绑定

- [BATCH84](BATCH84.md)：三个对象阶段buffer/bytes重载接通，真实payload让
  三次MeshDispatch三角形落在x≈80/220/360；13个畸形输入拒绝。原始剩余
  **60 bridge / 42旧chunk**，T70另计61/43。捕获`d29d5e86c1a9…`、
  库/app`aef27dba4f8d…`；84 captures、2141畸形、840 lifecycle全量通过，
  growth 1671168 bytes，日志`/tmp/metal-batch84-full.log`。
  GUI/Computer Use未运行，51份待人工；未提交/推送。

## 历史检查点：BATCH83 Object/Mesh pipeline 与对象buffer

- [BATCH83](BATCH83.md)：真实object shader的buffer→payload→mesh三角形链，
  同步object+mesh pipeline及object buffer绑定接通，16个畸形输入拒绝。
  原始剩余**63 bridge / 42旧chunk**，T70另计64/43。捕获`e7e52fa73e7d…`、
  库/app`77fd1d77b4cf…`；83 captures、2128畸形、830 lifecycle全量通过，
  growth 2179072 bytes，日志`/tmp/metal-batch83-full.log`。
  GUI/Computer Use未运行，50份待人工；未提交/推送。

## 历史检查点：BATCH82 异步 Mesh pipeline

- [BATCH82](BATCH82.md)：异步mesh pipeline的descriptor调用时快照与回调包装，
  原descriptor改动后仍捕获BGRA8/mesh函数/options；8个畸形输入拒绝。原始剩余
  **64 bridge / 42旧chunk**，T70另计65/43。捕获`5c89deaf9996…`、库/app
  `8d09e99f81c5…`；82 captures、2112畸形、820 lifecycle全量通过，
  growth 1982464 bytes，日志`/tmp/metal-batch82-full.log`。
  GUI/Computer Use未运行，49份待人工；未提交/推送。

## 历史检查点：BATCH81 Mesh thread-grid draw

- [BATCH81](BATCH81.md)：直接`drawMeshThreads`真实捕获/回放与MeshDispatch；
  32线程网格的绿色三角形、8个畸形输入拒绝。原始剩余**65 bridge / 42旧chunk**，
  T70另计66/43。捕获`e6126828325b…`、库/app`2eea4b68f328…`；定向通过，
  全量日志`/tmp/metal-batch81-full.log`，81 captures、2104畸形、810 lifecycle
  通过，growth 1212416 bytes。GUI/Computer Use未运行，48份待人工。
  未提交/推送。

## 历史检查点：BATCH80 Mesh texture/sampler 绑定

- [BATCH80](BATCH80.md)：六种mesh texture/sampler绑定、真实mesh shader采样
  使三次draw各右移约70像素，28个畸形输入拒绝。原始剩余**66 bridge / 42旧chunk**，
  T70另计67/43。捕获`28d188275e44…`、库/app`54f1bbde30f5…`；定向通过，
  全量日志`/tmp/metal-batch80-full.log`，80 captures、2096畸形、800 lifecycle
  通过，growth 1638400 bytes。GUI/Computer Use未运行，47份待人工。
  未提交/推送。

## 历史检查点：BATCH79 Mesh buffer/bytes 绑定

- [BATCH79](BATCH79.md)：四个mesh绑定bridge及新chunk，真实shader使用buffer
  offsets0/16/32与inline radius，三阶段左/中/右像素及seek已自动验证；16个
  畸形输入拒绝。原始剩余**72 bridge / 42旧chunk**，T70另计73/43。
  捕获`28f4ba76c665…`、库/app`4fc7b5c759ba…`；定向通过，全量日志
  `/tmp/metal-batch79-full.log`，79 captures、2068畸形、790 lifecycle全量通过，
  resident growth 1490944 bytes。GUI/Computer Use未运行，46份待集中人工。
  未提交/推送。

## 历史检查点：BATCH78 最小 Mesh pipeline 与直接 draw

- [BATCH78](BATCH78.md)：同步mesh pipeline、直接`drawMeshThreadgroups`真实
  capture/replay，MeshDispatch事件及三角形中心/背景像素验证；17个畸形输入安全拒绝。
  当前原始标记**76 bridge / 42旧chunk**；T70功能缺口另计77/43。
  T78 capture `160f902aef23…`，库/app `dfe3ec542c36…`；78 captures、2052畸形、
  780 lifecycle全量通过，growth 0 bytes，日志`/tmp/metal-batch78-full-final.log`。
  GUI/Computer Use未运行，
  T34–T69、T71–T78与T10 marker共**45份**待集中人工；T70不供UI打开。
  未提交/推送。

## 历史检查点：BATCH77 异步Tile pipeline

- [BATCH77](BATCH77.md)：异步Tile pipeline创建的调用时descriptor快照、
  原生回调包装与离线重建；8个畸形输入拒绝。T77 capture `0470cd9f7ecf…`，
  库/app `173fadcf7c8a…`。当前原始标记**78 bridge / 42旧chunk**；T70
  功能缺口另计为79/43。**77 captures API/CLI、2035畸形输入、770 lifecycle**
  全量通过，growth 2342912 bytes；日志`/tmp/metal-batch77-full-final.log`。
  GUI/Computer Use未运行，T34–T69、T71–T77与T10 marker共**44份**待集中人工；
  T70不供UI打开。未提交/推送。

## 历史检查点：BATCH75–76 Tile资源与动态内存

- [BATCH75](BATCH75.md)：六种Tile texture/sampler绑定真实capture/replay，
  采样纹理`2/255`驱动GPU计数；22个畸形输入拒绝。T75 capture
  `647f1bb524cb…`。T74夹具和捕获未变。
- [BATCH76](BATCH76.md)：Tile threadgroup memory长度、非零offset与零长度清除；
  tile shader实际使用动态内存，8个畸形输入拒绝。T76 capture
  `1ca3f463afa1…`。当前原始标记**79 bridge / 42旧chunk**；T70功能缺口
  另计则为80/43。
- T76全量通过：**76 captures API/CLI、2027畸形输入、760 lifecycle**，
  growth 1687552 bytes，日志`/tmp/metal-batch76-full.log`；T75全量亦通过
  （75/2019/750），库/app `8af6f90b645b…`。
  GUI/Computer Use未运行，T34–T69、T71–T76与T10 marker共**43份**待集中人工；
  T70不供UI打开。未提交/推送。

## 历史检查点：BATCH74 Tile pipeline与tile dispatch

- [BATCH74](BATCH74.md)：6 bridge / 6旧chunk真实capture/replay，剩余原始标记
  **86 bridge / 49旧chunk**；T70功能缺口另计则为87/50。三个tile dispatch真实
  GPU写buffer并驱动后续draw，事件网格和回退已自动核对。
- **74份capture API/CLI、1997畸形输入、740次lifecycle**全量通过，growth
  2064384 bytes；日志`/tmp/metal-batch74-full-final.log`，库/app`e74885bf9e09…`。
  GUI/Computer Use未运行，T34–T69、T71–T74与T10 marker共**41份**待集中人工；
  T70不供UI打开。未提交/推送。下一族优先Tile texture/sampler六重载，继续复用
  T74 pipeline并以真实texture采样验证，不做空绑定假测试。Heap alias/placement
  继续按[PLAN.md](PLAN.md)中的时序门槛处理。

## 当前恢复检查点：BATCH73 Render heap residency

- [BATCH73](BATCH73.md)：`useHeap/useHeaps`四种Render重载真实capture/replay，
  **92 bridge / 55旧chunk**原始标记；T70实际未接通范围另计则为93/56。
  **73份成功capture API/CLI、1969畸形输入、730次lifecycle**全量通过，
  growth 1015808 bytes；日志`/tmp/metal-batch73-full.log`，库/app `57efba093c53…`。
- GUI/Computer Use未运行，T34–T69、T71–T73与T10 marker共**40份**待集中人工；
  T70不供UI打开。未提交/推送。下一族优先继续评估Heap placement/alias生命周期或
  其他有真实应用阻塞的接口，不把原生透传当作回放支持。

## 当前恢复检查点：BATCH71–72 自动 Private Heap 的 Buffer/Texture

- [BATCH71–72](BATCH71-72.md)：`MTLDevice::newHeapWithDescriptor`及 Heap 子 buffer/texture
  形成真实 capture/replay 闭环；仅自动分配、Private、tracked hazard 的受控范围。
  **72份成功捕获 API/CLI、1947畸形输入、720 lifecycle** 全量通过，growth
  1556480 bytes；日志 `/tmp/metal-batch72-full.log`，库/app `c36b553dcd14…`，
  Max1286。原始标记 **96 bridge / 59旧chunk**；新增 Heap 包装显式暴露两个
  offset/placement 未接通入口，所以 bridge 原始数不等于本批净新增能力。
- T70 GPU indirect ICB range仍明确拒绝且单独复验通过；不计入成功回放。
  GUI/Computer Use未运行，**T34–T69、T71/T72与T10 marker共39份**待集中人工；
  T70不供UI打开。未提交/推送。下一族考虑 Render `useHeap(s)` 四重载。

## 当前恢复检查点：BATCH70 GPU 间接 ICB 范围安全边界

- [BATCH70](BATCH70.md)：原始标记 **95 bridge / 60旧chunk**；其中间接 ICB 范围
  是原生执行、可截帧、离线明确拒绝的**未解决功能**，所以按真正接通功能的口径仍为
  **96/61**。T70 GPU 写入范围的 Metal Validation 原生/捕获、结构检查、预期
  replay 拒绝通过；T53/T67 旧 capture 定向 API/CLI 通过。正确事件树仍需执行点
  范围方案，见 [PLAN.md](PLAN.md)。
- T01–T69 + T10 marker 联合终端回归通过：**70份 capture API/CLI、1917畸形输入、
  700次 lifecycle**，growth 1802240 bytes；日志 `/tmp/metal-batch70-full.log`，
  库/app `6ba534c557b2…`。T70 是单独的预期失败负例，不计入成功回放。
- GUI/Computer Use 未运行；T70 不可离线加载，不加入人工 QA；既有37份待验不变。
  未提交/推送。

## 当前恢复检查点：BATCH69 Intersection Function与SharedEvent handle

- [BATCH69](BATCH69.md)：接通3 bridge / 2旧chunk，剩余 **96 bridge / 61旧chunk**；
  T55同步/异步intersection function链接compute pipeline，T69同进程SharedEvent handle
  导入与跨queue时间线。**70 captures API/CLI、1917畸形输入、700 lifecycle**全量通过，
  growth 770048 bytes；日志`/tmp/metal-batch69-full.log`，库/app `22c244afc13a…`。
- GUI/Computer Use未运行；**T34–T69 + T10 marker共37份**待集中人工；未提交/推送。
- ICB GPU indirect执行范围仍优先于扩大ICB范围，见[PLAN.md](PLAN.md)。

## 当前恢复检查点：BATCH67–68 patch draw重载与同进程shared handle

- [BATCH67–68](BATCH67-68.md)：接通5 bridge / 5旧chunk，剩余 **99 bridge / 63旧chunk**，
  Max1284不变。T67三种patch draw及T68同进程shared-texture handle实录和定向回放通过。
  **69 captures API/CLI、1903畸形输入、690 lifecycle**通过，resident growth 786432 bytes；
  日志`/tmp/metal-batch68-host-final.log`，库/app `f8b8b15d458…`。
- GUI/Computer Use未运行；**T34–T68 + T10 marker共36份**待集中人工；未提交/推送。
- 下一开发波次优先：ICB GPU indirect执行范围。必须以GPU执行点范围构建事件树；
  未通过GPU生成range真实夹具前继续拒绝，见[PLAN.md](PLAN.md)。

## 历史检查点：BATCH66 直接 patch tessellation

- [BATCH66](BATCH66.md)：factor buffer、scale、直接 `drawPatches` 三个 bridge 与三个旧 chunk
  已接通；剩余 **104 bridge / 68旧chunk**，Max1284不变。**67 captures API/CLI、
  1883畸形输入、670 lifecycle**通过，resident growth 1376256 bytes；日志
  `/tmp/metal-batch66-host-final.log`，库/app `d098a04a1a7b…`。
- GUI/Computer Use未运行；**T34–T66 + T10 marker共34份**待集中人工；未提交/推送。
  GPU生成的ICB indirect执行范围仍未解决，不从编码时CPU值推断事件树。

## 历史检查点：BATCH65 SharedEvent初值与GPU同步集中自动通过

- [BATCH65](BATCH65.md)：SharedEvent创建及旧chunk1033接通，首次GPU使用前CPU初值可回放，
  T63动态stride补5个布局负例；剩余 **107 bridge / 71旧chunk**，Max1284。
- **66 captures API/CLI、1872畸形输入、660 lifecycle**通过，growth2162688 bytes；
  日志 `/tmp/metal-batch65-host-final.log`，库/app `d69fa7da1f8…`。
- GUI/Computer Use未运行，**T34–T65 + T10 marker共33份**待集中人工；未提交/推送。
  SharedEvent仅同进程GPU时间线与首次GPU使用前CPU初值；后续CPU赋值与handle导出
  会被明确标记为不可离线回放。
  以下为历史检查点。

## 历史检查点：BATCH63–64 集中自动通过

- [BATCH63–64](BATCH63-64.md)：动态顶点stride四重载、单视图amplification和descriptor-backed
  Private shared texture接通；净减 **6 bridge / 2旧chunk**，剩余 **108/72**，Max1282。
- **65 captures API/CLI、1855畸形输入、650 lifecycle**在最终代码上通过，resident
  growth2392064 bytes；日志 `/tmp/metal-batch64-final.log`，库/app `8772ed87e268…`。
- GUI/Computer Use未运行，**T34–T64 + T10 marker共32份**待集中人工；未提交/推送。
  单视图和Private descriptor是明确边界，不代表multiview或shared handle已支持。
  以下为历史检查点。

## 历史检查点：BATCH62 集中自动通过

- [BATCH62](BATCH62.md)：14 个 bridge 与 2 个旧 chunk 接通，剩余 **114/74**，
  Max1278。设备纯查询/宿主调度无需 replay chunk；buffer/texture purgeable 只保证
  KeepCurrent、NonVolatile 离线回放，Volatile/Empty 明确不支持。
- **63 captures API/CLI、1784 异常、630 lifecycle** 在最终代码上通过，growth606208bytes；
  日志 `/tmp/metal-batch62-final-replay.log`。库/app `e11191df7a77…`，GUI executable 未变。
- GUI/Computer Use未运行，**T34–T62 + T10 marker 共30份**仍待集中人工；未提交/推送。
  下方为历史检查点。

## 当前恢复检查点：BATCH61 no-copy Buffer集中自动通过

- [BATCH61](BATCH61.md)：真实no-copy创建及旧chunk1006接通，当前剩余**128 bridge /
  76旧chunk**，Max1278；只承诺Shared默认options。应用指针和deallocator保留在原生抓取，
  离线回放复制字节。抓取保留资源可能延后deallocator，详情见批次边界。
- 完整一键**62 captures API/CLI、1772异常、620 lifecycle**通过，growth1327104bytes；
  日志`/tmp/metal-batch61-final.log`，库/app `0f7f42e63b34…`，GUI executable未变。
- GUI/Computer Use未运行，**T34–T61 + T10 marker共29份**仍待集中人工；未提交/推送。
  更高级剩余族应围绕真实捕获阻塞挑选；无需为了清零标记透传无法回放的对象。
  以下为历史检查点。

## 当前恢复检查点：BATCH60 Device ArgumentEncoder集中自动通过

- [BATCH60](BATCH60.md)：Device descriptor创建的独立ArgumentEncoder接通1 bridge /1旧chunk，
  仅只读2D纹理和sampler；总剩余**129/77**，Max1278。真实shader消费两个packet，
  身份/布局29异常和回退通过，其他布局明确拒绝。
- 一键完整门禁**61 captures API/CLI、1759异常、610 lifecycle**通过，growth1130496bytes，
  日志`/tmp/metal-batch60-final.log`；库/app `ad4e54209464…`，GUI executable未变。
- GUI/Computer Use未运行，**T34–T60 + T10 marker共28份**仍待人工；未提交/推送。
  下一波应按真实捕获阻塞或剩余高级功能族选题，不将无测试的标记移除当作接通。
  以下是历史检查点。

## 当前恢复检查点：BATCH58–59 共享存储纹理族集中自动通过

- [BATCH58](BATCH58.md)：三种纹理view与buffer-backed纹理接通**6 bridge、4旧chunk**，
  剩余**130/78**，Max1278未变。父资源初值、提交间CPU更新及回退语义已补齐；支持范围见批次记录。
- 原生/capture、逐事件API/CLI、91个新增畸形变体及旧T定向通过。共享资源改动后立即
  集中回归：**60份capture API/CLI、1730畸形样本、600 lifecycle**，growth524288bytes；
  日志`/tmp/metal-batch58-59-final.log`。库/app SHA `474533dbd0be…`，GUI executable未变。
- GUI/Computer Use未运行，**T34–T59 + T10 marker共27份**仍待集中人工；未提交/推送，
  用户UE路线保留。下一个候选是Device独立ArgumentEncoder的descriptor创建族，需先确认
  其布局、成员验证和真实shader消费，不为减少标记而直接透传。以下均为历史检查点。

## 当前恢复检查点：BATCH57 Argument数据族集中自动通过

- [BATCH57](BATCH57.md)为本波唯一详细记录：buffer成员/批量、constants、arrayElement，
  多packet与GPU地址重定位、CPU提交更新/seek；**140→136 bridge，旧chunk仍82**，Max1278。
- 共享Shared初值恢复影响较广，本族立即集中回归：**58 captures API/CLI、1639异常、
  580 lifecycle**通过，30份API验证层，growth2555904bytes。另三份旧源码新录兼容、
  T35新录与更严格的首dispatch初值回退通过；无本批计划内自动回归欠项。
- 新增161异常、GPU地址清零后的合法重定位正例；真实帧内资源重绑native正常、离线拒绝。
  仅顶层Shared目的buffer/只读成员/FS packet；不承诺nested、GPU生成、VS/CS或任意重绑。
- 日志 `/tmp/metal-batch57-final.log`；库/app `8e84e249d684…`；GUI `fe8bcf852b68…`，
  只补buffer成员槽位映射并编译，未运行。UI待验 **T34–T57+T10 marker，共25份**，
  总单保留全部旧项并补T57/T35差异；未使用Computer Use、未提交，用户UE路线保留。
- 下一编号T58，候选texture views/共享存储别名，先审查初值及双向写入语义。
  以下均为历史检查点。

## 上一检查点：BATCH54–56 三族集中自动通过

- [BATCH54-56](BATCH54-56.md)为本波唯一详细记录：重载兼容1bridge/3旧chunk，Function
  创建4bridge/2旧chunk，Argument texture/sampler数组及reflection创建3bridge。
  **148→140 bridge，87→82旧chunk**；仅追加async1272/1273，Max1274。
- 三fixture各native/capture6帧；集中**57 captures API/CLI、1478异常、570 lifecycle**通过，
  29份API Metal验证层，growth1671168bytes；另3份旧源码新录兼容通过。无自动回归欠项。
  日志 `/tmp/metal-wave54-56-final.log`；库/app `71cf518d1e5b…`，GUI executable未改。
- C已明确收窄；下波优先单独处理argument buffer成员/constants/arrayElement及GPU地址
  重定位、CPU初值/提交更新/回退，不可仅移除bridge标记。下一编号T57。
- UI待验 **T34–T56 + T10 marker共24份**，最小差异并入QA_CONSOLIDATED，全部保持开放。
  未启动GUI/Computer Use、未提交；旧修改及用户UE路线保留。以下均为历史检查点。

## 上一检查点：推进节奏调整

- 执行规则集中于PLAN“当前推进节奏”；连续2–3族、定向检查、集中关口，最多3族验证间隔。
  以后此处只保留短检查点，不继续复制完整批次内容到多份文档。
- 本次仅改推进规则与新增 `test_metal_replay_targeted_macos.sh`，无Metal功能/格式变更。
  快速入口9份代表capture的API Metal验证层与CLI通过，重复T53已去重；参数/缺失capture
  错误非零退出、bash语法及diff检查通过。日志 `build-macos-debug/metal-targeted.IAT5mF/`。
- 最近集中基线仍是BATCH53：148 bridge / 87旧chunk，54 captures/1330异常/540 lifecycle。
  本波次新增功能族0，未产生新的驱动回归欠项；本次没有重跑1330异常/540 lifecycle。
- 下一步：在一份活跃BATCH表中确定首批重载兼容/Function创建/Argument encoder的边界与
  必跑旧T，再连续实现；编号从T54起。真实依赖或失败可调整顺序，不强凑bridge数量。
- GUI待验仍为T34–T53+T10 marker共21份，见QA_PENDING/QA_CONSOLIDATED；未启动GUI，
  未使用Computer Use、未提交。下方BATCH53及更早条目保留为历史证据。

## 2026-09-26 BATCH53 自动完成：ICB GPU 操作与可靠回退

- 单命令 reset 接通 1 bridge；GPU ICB reset/copy/optimize 接通旧 chunk1224–1226。
  剩余 **bridge 148 / 旧 chunk 87**。追加1270 reset、1271 初值不可用诊断，Max1272。
  后三项 bridge 原本已转发，不重复计算减少量。详见 BATCH53 / PHASE53。
- CPU 初始化的 Shared render ICB 支持跨 ICB/同 ICB 非重叠 copy、GPU reset/optimize、
  空命令与单命令 reset/re-encode；每个 replay epoch 恢复初值，保留 copied PSO/buffers/
  indexed 状态。捕获前 GPU 内容不可重建时明确拒绝，不声称通用 GPU-generated ICB 支持。
- T53 native/capture 各12帧、四阶段像素、9 draw / 5 empty、资源状态和往返均通过。
  新增194异常+3合法变体，纳入旧70 ICB异常+2空命令正例；旧缺失pipeline崩溃已修复。
  **54 captures API/CLI、1330 类异常、540 次 lifecycle**通过，growth **2,228,224 bytes**；
  26份 API Metal验证层。另帧前GPU写入负例native/capture各3帧正常，离线明确拒绝。
- 最终日志 `/tmp/metal-batch53-final.log`；一键 `test_metal_capture_batch53_macos.sh`，
  仅 replay 设 LAST_TEST=53。库/app `a5dcf57234590…`，T53 `6ad98408529a…`；GUI未改。
- 无GUI/Computer Use、无提交，保留旧修改和用户UE路线。**T34–T53 + T10 marker 共21份**
  待集中人工，最小差异已并入 QA_CONSOLIDATED；PHASE35–53 仍开放。下一编号 T54/PHASE54。
  以下为历史检查点，不代表当前剩余计数。

## 2026-09-26 BATCH51–52 自动完成：bridge 已降到 149

- 新增 9 bridge：T51 六个异步 library/render/compute 创建入口；T52 newEvent、signal、
  wait。剩余 bridge **158→149**，旧 chunk 未处理分支 **93→90**；新增可回放 async
  chunk1264–1269，Max1270。详见 BATCH51-52、PHASE51、PHASE52。
- T51 保留原生 callback/error/reflection、复制 descriptor、登记父依赖、处理借用结果
  所有权；五个 PSO 均实际执行。Source async 离线仅支持 options=nil，不运行应用 block。
  扩展 PSO/Event 独立 native 引用，补 source 资源校验及 Device 类型初始化。
- T52 原生编码 signal/wait，按 replay epoch 重建 Event 防止回退残留；验证两队列三
  提交的 GPU 数据链。仅支持已捕获先 signal 后 wait，外部/SharedEvent/future-signal
  wait 拒绝；不是任意并行调度重构。未支持路径不以空实现冒充成功。
- 最终 **53 captures API/CLI、1066 类异常、530 次 lifecycle** 通过，resident growth
  **1,736,704 bytes**；18 份 API Metal 验证层。新增 226 类异常（104+122）和四个合法
  变体；旧正式 captures 未重录，另三份 source 新录兼容 API/CLI 通过。
- 库/app副本 `8dbbe2d60146…`；T51/T52 `e0fc940f6ce0…` / `e0116d679d5f…`；GUI
  executable 未改。日志 `/tmp/metal-batch51-52-final.log`，一键入口
  `test_metal_capture_batch51_52_macos.sh`，仅 replay 设 LAST_TEST=52。
- 无 GUI/Computer Use、无提交，用户 UE 路线和原有 dirty 修改保留。**T34–T52 + T10
  marker 共 20 份**待人工，QA_CONSOLIDATED 已合并最小 UI 差异；PHASE35–52 仍开放。
  下一编号 T53/PHASE53。以下是历史检查点，不代表当前剩余计数。

## 2026-09-26 BATCH50 自动完成：两种纹理CPU读取与子资源同步

- T50接通2 bridge / 3旧chunk1072/1073/1205，剩余 **158/93**，Max1264不变。
  CPU getBytes保留原生透传，新增帧内元数据/资源引用；离线不写应用CPU指针。
  synchronizeTexture真实编码并检查Managed/当前encoder/子资源；Shared同步明确拒绝。
- 四次CPU读回覆盖局部2D/mip/array slice、3D两层、host行/图像padding和哨兵；native及
  capture各12帧48次读取通过。API核对独立读/同步纹理保留、2D子资源、CPU派生68-byte
  参数/回退/usage、RGBA43/79/113/255。3D viewer未扩展，边界见PHASE50。
- **51 captures API/CLI、840类异常、510次lifecycle**通过，growth **999,424bytes**；
  16份API Metal验证层。新增176异常+2合法变体。最终一键/日志/完整hash见BATCH50，
  只重放LAST_TEST=50；旧正式capture不重录。
- 库/app `a648ce05f576…`，T50 `f56406953e8d…`。无GUI/Computer Use、无提交。
  全部待人工 **T34–T50 + T10 marker，18份capture**，阶段和批次保持开放。

## 2026-09-26 BATCH49 自动完成：两回调入口与CPU提交/回退修复

- T49 scheduled/completed handlers：2 bridge、旧chunk1049/1054，剩余 **160/96**；
  Max1264不变。Native回调保持包装身份/生命周期，离线只呈现注册记录，不执行应用block。
- 修复commit后取CPU快照的竞态、submission更新在部分replay中太晚应用，以及CPU更新
  buffer回退残留；payload改为安全读取并校验范围/身份/实际长度。边界见PHASE49。
- 两command buffer、八回调、两个dispatch/三个fill/一个draw，callback前后12-byte参数、
  412-byte output及padding、反射/绑定、RGBA41/67/101/255和事件回退通过。
- **50 captures API/CLI、664类异常、500次lifecycle**通过，growth **3,178,496bytes**；
  15份Replay API Metal验证层。最终一键含三份源码新录兼容检查，全部使用同一库。
  入口`test_metal_capture_batch49_macos.sh`，日志和版本见BATCH49；仅重放LAST_TEST=49。
- 库/app `1cd92c21c774…`，T49 `82771996ea03…`。无GUI/Computer Use、无提交。
  全部待人工 **T34–T49 + T10 marker，17份capture**；阶段保持开放，集中总单保留全部旧项。

## 2026-09-26 BATCH48 自动完成：4个bridge、4个旧chunk与shader生命周期修复

- T48 file/URL/data/bundle binary library，旧chunk1015–1018接通；Max1264不变。
  剩余 **162 bridge / 98 chunk**。默认库读取、失败返回值以及library/function提前释放
  的heap corruption一并修复；其他类型wrapper所有权尚未重构，详见PHASE48。
- 五个库均实际执行，首帧前释放全部源对象；移走原metallib/app/bundle仍能回放。
  五份10765-byte嵌入payload逐字节核对、676-byte输出/padding、反射/依赖/回退/像素通过。
- **49 captures API/CLI、577类异常、490次lifecycle**通过，growth **1,228,800bytes**；
  14份Replay API Metal验证层。另三份源码路径新录兼容capture各API与3-loopCLI通过。
  主batch和补充检查分段完成且已整合脚本，日志/范围见BATCH48；只重放LAST_TEST=48。
- 库/app `6df633b629eb…`，T48 `f71eaf76384b…`。未运行GUI/Computer Use，未提交。
  全部待人工 **T34–T48 + T10 marker，16份capture**，阶段和批次不关闭。
  二进制库无MSL源文件属预期；动态库/跨OS-GPU/真实UE支持不在本批承诺。

## 2026-09-26 BATCH47 自动完成：再接通3个bridge、3个旧chunk

- 同步render options/reflection、compute function options/reflection、compute descriptor。
  旧chunk1022/1024/1025接通，Max1264不变；剩余 **166 bridge / 102 chunk**。
- T47六个pipeline，原生reflection及结构体成员、descriptor线程数/整倍数/mutability，
  三次dispatchThreadgroups加一次dispatchThreads、两个draw；532-byte数据、20-byte padding、
  pipeline身份/绑定/反射/usage和事件回退、最终RGBA17/41/73/255自动核对。
- 最终一键batch通过：**48 captures API/CLI、513类异常、480次lifecycle**，resident growth
  **491,520bytes**；13份Replay API Metal验证层。T47新增84负例及options1/2合法变体。
  `test_metal_capture_batch47_macos.sh` / `/tmp/metal-batch47-final.log`；只重放设LAST_TEST=47。
- 库/app `16c921313c2f…`，T47 `a28eef0f401b…`；完整hash/范围见BATCH47、PHASE47。
  未运行GUI/Computer Use，未提交。全部待人工 **T34–T47 + T10 marker，15份capture**。
  高级descriptor/异步pipeline不在支持范围，不宣称真实UE兼容；阶段继续开放。

## 2026-09-26 BATCH45–46 自动完成：再接通7个bridge、9个旧chunk

- T44新增Fence包装/创建和Blit/Compute/Render update/wait；T45/T46新增两种定时present、
  buffer添加/清空debug marker。合计11入口，Compute追加chunk1262/1263；剩余 **169/105**。
- Fence执行真实GPU同步，检查类型/active encoder/先行update与replay epoch，拒绝无效
  等待；marker保留frame引用并清理被后台reset覆盖的注释。timed present保留参数及输出
  action，离线replay不等待原机器时钟。不宣称跨队列/外部fence或高级stage已支持。
- 完整 `test_metal_capture_batch45_46_macos.sh` 通过：三模式native验证层/capture/XML/
  3-loopCLI、精确GPU数据/usage/seek，最终 **47份API/CLI、429类异常、470次lifecycle**；
  resident growth **1,638,400bytes**。12份replay API验证层；T00空帧另做CLI/40次定向生命周期。
- 库/app副本 `33c4399361f7…`；T44/T45/T46为 `caa028676f68…` / `9e71f6b21883…` /
  `7b1fbea45e1f…`。详情 `BATCH45-46.md`、PHASE45/46；仅重放入口设LAST_TEST=46。
- 全部待人工 **T34–T46 + T10 marker，14份capture**，总单QA_CONSOLIDATED保留全部旧项。
  GUI/Computer Use未执行，阶段/批次保持开放；没有提交或覆盖既有UE路线。

## 2026-09-26 BATCH43–44 自动完成，集中 GUI QA 扩展至十一份

- T42新增七个compute resource/barrier/marker入口；T43新增五个render分阶段声明/barrier
  入口，保留声明-only资源与初始内容。新chunk1255–1261追加。剩余bridge/chunk **176/114**。
- 修复失败render pipeline创建包装空对象；补齐vertex RW buffer的descriptor、storage
  分类和usage，以及VS Storage Buffers表入口。UI源码已构建，未做GUI验证。
- native验证层/capture/XML/3-loopCLI与逐事件buffer/pixel/descriptor通过；最终联合
  **44 captures API/CLI、342类异常、440次lifecycle**通过，resident growth **2,899,968 bytes**。
  共享脚本加入T12/T19/T42/T43验证层，目前9份replay验证层覆盖。修正旧harness样例识别
  和缩略图假设，不减去旧fixture断言；证据见 `BATCH43-44.md`。
- 正式库/app副本 `736925e6e195…`，app executable `3cc9c3b63507…`；
  T42/T43 captures `25c6e7349f22…` / `7139504e2357…`。新一键入口
  `test_metal_capture_batch43_44_macos.sh`，仅重放设LAST_TEST=43。PHASE43/44列边界。
- **T34–T43 + T10 marker十一份**仍待人工，统一总单 `QA_CONSOLIDATED.md`。
  全部未验阶段保持开放；无Computer Use、无GUI、无提交；原有UE路线和编辑保留。

## 2026-09-26 BATCH41–42 自动完成，继续累积集中 GUI QA

- T40接通compute inline bytes、buffer offset、动态threadgroup memory三个此前隐式转发
  的入口；新chunk1252–1254追加。五次reduction验证CPU数据拷贝、真实/inline切换、两个
  encoder状态重置、offset/size事件快照、结果与padding。dispatch校验使用reflection及
  真实pipeline/device限制，含static+dynamic memory总量；旧compute布局限制仍保留。
- T41接通blit descriptor创建和四种CPU/GPU优化提示，补frame引用、修复CPU slice/mip
  写错chunk ID。额外13×7纹理只由hint引用，捕获/回退内容正确。带counter sample buffer
  的descriptor明确拒绝。剩余bridge/chunk **181/119**；计数不包含无旧标记的compute入口。
- 新两项Metal API Validation native、capture/XML、Replay API、3-loop CLI通过。
  最终整批脚本通过 **42 captures API/CLI、239类异常、42×10 lifecycle**，resident growth
  **2,736,128 bytes**（最终显示布局修复后，门限64MiB）；脚本/源码格式检查通过。
  T40/T41 hashes `73a3398cec48…` / `35745dc5f7ab…`。
- 补跑replay验证层发现并修复共有display参数60/64-byte布局差异，显式padding+编译断言；
  T01/T02/T09/T40/T41验证层均通过，新增到共享脚本。当前库/app更新为`b30a0e4cb6ee…`，
  上述最终联合回归已在新库上再次通过；802,816bytes属padding修复前整批结果。
- 一键入口 `test_metal_capture_batch41_42_macos.sh`；仅重放设LAST_TEST=41，详见
  `BATCH41-42.md`、PHASE41/42。集中QA扩展为 **T34–T41 + T10 marker九份capture**，
  见 `QA_CONSOLIDATED.md`。全部GUI未执行，阶段/批次保持开放；没有Computer Use、没有
  启动qrenderdoc、没有提交。原有UE路线及所有未提交编辑保留。

## 2026-09-26 BATCH39–40 自动完成，已合并后续 GUI QA

- T38 接通 VS/FS/CS single/batch sampler LOD 六个入口；有效 descriptor 按事件保存，
  普通 rebind 恢复静态范围，nil/batch 空槽和各 stage 不串状态。新增两个 compute 尾部
  chunk，不重编号旧 ID。剩余 bridge/chunk **182/124**（本会话基线216/165）。
- T39 接通 private GPU buffer staging readback，验证非对齐字节区间、末尾截断、非法
  资源/范围与事件回退。修复 direct compute 原先只接受纹理滤镜布局的问题，新增
  texture→buffer/buffer-only 分支和 reflection 绑定/大小/对齐检查；旧路径约束仍保留。
- 新两项 native/capture/XML/API/3-loop CLI 通过。最终联合入口覆盖 T01–T39 + T10_debug
  **40 captures API/CLI、175类异常检查、40×10 lifecycle**；lifecycle 新增同进程重复
  Private/Shared 非对齐读回，最终 resident growth **557,056 bytes**（门限64MiB）。
  库与 app 内嵌库同为 `35150e09a506…`。批次说明 `BATCH39-40.md`，新样例 T38/T39 SHA 为
  `92d869806241…` / `0d552603e709…`。
- `QA_CONSOLIDATED.md` 已把 T34–T39 + T10 marker 七份待验 capture 合并为同一 app
  验收顺序；公共操作只做一次。用户将在重置后集中 QA，现在不催促、不做 Computer Use。
  当前库版本见该总单；旧单 hash 属于历史检查点。PHASE35–40、相关三批均保持开放。
  全部修改未提交，原有 UE 路线与其他编辑保留。

## 2026-09-26 BATCH38 自动完成，累计待人工 T34–T37/T10 marker

- 在 BATCH35–37 之后继续扩展：八项 visibility/store/barrier render 入口、四项 blit
  双向/whole/range copy 实际 replay、三项 blit debug chunk；剩余 bridge/chunk 为
  **186/128**（会话基线 216/165）。同时修复 visibility buffer 漏捕获导致的 native
  replay 崩溃和 Unknown store 在提前结束 pass 时的风险；新增 D32S8 raw readback/picking。
- T36/T37/T10_debug 原生、capture/XML、Replay API、逐份 3-loop CLI 通过；最终库上
  T01–T37 + T10_debug 共 38 captures API/CLI 回归通过，38 × 10 lifecycle 通过，
  最终脚本 resident growth 2,064,384 bytes（门限 64 MiB）。
  T34/T35 14、T36 14、T37 43，共 71 类异常输入被干净拒绝；详见 `BATCH38.md`。
  统一复跑入口 `test_metal_replay_batch35_38_macos.sh`；不重新录制历史 captures。
- 最终库与 app 内嵌库 SHA `47e0635e8736…`；T36 `436b3262fef5…`、T37
  `f87cbbc8ca00…`、T10_debug `2d4a8d609bae…`；T34/T35 不变。旧 T10 capture 保留。
- 没有启动 qrenderdoc 或使用 Computer Use，没有提交或发布；原有 UE 路线等修改保留。
  PHASE35–38、BATCH35–37/BATCH38 均保持开放。后续读 `QA_BATCH35-37.md` 与
  `QA_BATCH38.md`，一次指导用户验证。下方 BATCH35–37 是后续扩展前的历史检查点，
  当前版本/计数以上述条目为准。

## 2026-09-26 BATCH35-37 自动完成，T34/T35/T36 GUI L4 延后

- T34 接通 render vertex/fragment inline bytes、buffer arrays 与 vertex offset；
  T35 接通 limited queue、descriptor/unretained command buffer 和两种 compute
  encoder 创建及 `waitUntilScheduled`；T36 接通 viewport/scissor arrays、depth
  clip/bias、triangle fill、blend constant，以及 command buffer/render encoder 的
  debug group/signpost。bridge/chunk 未接通计数从 216/165 降为 194/143。
- 两个终端 batch 均通过：三项 native、capture/XML、Replay API、逐份 3-loop CLI，
  T34/T35 共 14 类和 T36 共 5 类异常捕获拒绝；最终 T34/T35/T36 × 10 轮
  lifecycle 最后一轮 resident growth 606,208 bytes。共享 render 路径的 T01/T02/T04/
  T07/T17 Replay API 与 CLI 通过；T00、这些旧场景及 T34/T36 的 8 captures × 10
  lifecycle 通过，growth 245,760 bytes。qrenderdoc app/renderdoccmd 仅从终端构建，
  未启动 GUI；`git diff --check` 通过。
- 最终 replay 库及 app 内嵌库 SHA-256 `ba380a0317b6…`；T34/T35/T36 captures
  分别为 `e6ee77deb50a…`、`7fbed8c95df6…`、`37d6c5cb1306…`。批次说明见
  `BATCH35-37.md`，自动测试入口与后续 GUI 步骤见 `QA_BATCH35-37.md`。
- 按用户要求本轮不使用 Computer Use。`QA_PENDING.md` 已登记 T34/T35/T36；
  PHASE35–37 与 BATCH35-37 保持开放。此前 UE 5.6.1 路线记录及其他未提交改动保留。

## 2026-09-26 已定位 UE 5.6.1 官方 Mac 构建

- 用户提供 `/Volumes/CauseUseMac/UE_5.6`。只读检查确认官方 promoted
  UE 5.6.1、arm64 `UnrealEditor` 存在，足够用作普通帧和后续 Nanite
  最小场景试截的固定目标。尚未启动 UE 或试截帧。
- 安装内 `RenderDocPlugin.uplugin` 的 `PlatformAllowList` 只有 Win64/Linux；
  Mac 首轮应从终端启动外部注入路径，不预期现有插件按钮可用。
  Epic UE 5.6 Mac 要求列 M2+ Nanite/VSM Beta。细节见
  `REAL_WORLD_CAPTURE_ROADMAP.md`。当前 `QA_PENDING.md` 无人工待验项。

## 2026-09-26 真实应用截帧与高级功能路线评估

- 用户目标扩展到 macOS 图形应用/游戏、UE/Unity 集成、Nanite，以及光追、
  mesh shading 和更多 Metal 特性。当前 T00–T33 受控 capture/replay 闭环
  不能直接推断真实引擎帧兼容；优先建议试 UE 普通场景首帧，再以实际阻塞
  API 推进 Nanite 最小场景。评估与更省重复人工 L4 的节奏见
  `REAL_WORLD_CAPTURE_ROADMAP.md`。
- 本机 M2 Pro/16 GB，原生设备报告 Apple8、Metal3、ray tracing API；Apple
  功能表允许在此开发基础 mesh shading 与 ray tracing API，Apple9+ 的
  indirect mesh draw/mesh ICB 及 M3/M4 硬件加速/性能须换目标机验证。
  当前 RenderDoc wrapper 显式将 ray tracing 能力返回 false，高级 capture
  路径尚未实现。Epic UE 5.8 Mac 文档列 Nanite/VSM 为 M2+ Beta。
- 本次仅做只读代码/设备/外部文档检查并记录路线，没有改实现、captures
  或 GUI；`QA_PENDING.md` 仍无人工待验项，全部未提交改动保留。

## 2026-09-26 T32/T33 GUI L4 通过，BATCH33-34 关闭；action 名称审查

- 用户确认 T32 EID 12 与 T33 EID 18 的新版间接摘要
  `dispatchThreadgroups(indirect, <2, 2, 1>)` 符合预期。结合此前已验的参数、
  CS 间接参数栏、资源、画面和状态栏，T32/T33 GUI L4 均通过。
  PHASE33、PHASE34、BATCH33-34 已关闭；`QA_PENDING.md` 当前无待验项。
- 审查 Event Browser 的 custom action names：T32/T33 间接 dispatch 与 Vulkan/
  D3D11/D3D12/GL 展示实际 group 数的惯例一致；部分 Metal 直接 draw/dispatch、
  单次 indirect draw、ICB 和 blit 摘要较其他后端冗长或格式不统一。
  具体对照、字段取舍与后续定向验证计划见 `ACTION_NAME_ALIGNMENT.md`。
- 此轮只做代码审查与文档归档，没有改代码、正式 capture 或 app；沿用下方
  最终联合验证证据，未重跑测试。全部未提交改动保留。

## 2026-09-26 T32/T33 间接 dispatch 摘要对齐其他 API

- 对照 Vulkan、D3D11、OpenGL 和 D3D12，间接 dispatch 的 Event Browser
  自定义摘要突出参数 buffer 中实际执行的 threadgroup 数量。Metal 现显示
  `dispatchThreadgroups(indirect, <2, 2, 1>)`；buffer、offset 和
  `{4,4,1}` 每组线程数仍由 API Inspector/CS 栏位提供。
- T33 的参数由 GPU writer 产生：载入阶段在 command buffer 完成后读取参数
  并更新对应 action 的摘要与 dispatchDimension，避免显示写入前零值。
  T32/T33 Replay API 断言均确认摘要及三项实际执行数；T01/T10/T11/T13/
  T21/T28/T29/T30/T31/T32/T33 共 11 份定向 Replay API 与逐份 CLI 通过，
  含 T00 的 12 份 capture × 10 轮 lifecycle 通过，resident growth
  360,448 bytes；T32/T33 的 5/6 类异常捕获继续被拒绝。
- 最新 GUI `f1ebea2ddb86…`、库 `aa21c9a6983d…`；正式捕获不变。
  用户已确认此前 CS 栏位及其他 GUI 项，仅新版 Event Browser 摘要待最短
  复验。T32/T33 仍在 `QA_PENDING.md`，两阶段和批次保持开放；全部未提交
  改动保留。

## 2026-09-26 T32/T33 GUI 部分通过与 compute indirect 栏位修正

- 用户确认 T32 EID 6/12 及后续、T33 EID 7/10/12/18 及后续的参数数值、
  offset、CS 绑定、纹理、最终画面和状态栏；指出 compute indirect buffer
  被错误放进 IA。Metal Pipeline State 已把 compute 间接参数移至 CS 的
  `Indirect Dispatch`，图形 draw 的 IA `Indirect Buffer` 保持原位。
- Event Browser 依通用设置显示自定义 action 名称；API Inspector 保留完整调用
  参数。Metal compute 自定义摘要现含 offset 16 和 4×4×1 threads/group。
  Vulkan 的间接 dispatch 也使用自定义摘要显示解析后的执行参数，而非照抄
  API 调用。用户仍须在新 GUI 上确认这两处可见行为。
- 最终 GUI `f1ebea2ddb86…`、库 `9c0b65bea9f5…`；正式 T32/T33 captures
  SHA 保持 `9aa1432c8d9a…` / `4478197f9994…`。T11/T13/T21/T32/T33
  Replay API 与逐份 CLI、含 T00 的 6 份 capture × 10 轮 lifecycle 通过，
  `git diff --check` 通过。新最短复验见 `QA_BATCH33-34.md` 顶部，
  `QA_PENDING.md` 分别保留 T32/T33 待验；PHASE33/34、BATCH33-34 不关闭。

## 2026-09-26 BATCH33-34 P33.1–P34.4 自动验证完成，T32/T33 L4 待验

- T32 CPU shared Buffer 21 byte offset 16 的 `2,2,1` 间接参数与 T33 GPU writer
  在同一 command buffer、跨 compute encoder 生成 Buffer 23 的相同参数均完成。
  T33 帧内 blit 先清零，Replay API 已核对写入前零值、writer EID 10 后
  `2,2,1`、EID 18 间接 dispatch 后 8×8 纹理与输出 buffer，以及前后 seek。
  T32 Begin/dispatch EID 6/12；T33 writer/第二 Begin/dispatch EID 10/12/18。
- 原生 T32/T33、正式 capture/XML、参数/descriptor/usage/readback/action、
  T32 五类及 T33 六类异常拒绝均通过。最终库上 T01/T10/T11/T12/T13/
  T16/T17/T18/T19/T21/T28/T29/T30/T31/T32/T33 共 16 份 Replay API 与
  逐份 CLI replay 通过；含 T00 的 17 份 capture × 10 轮 lifecycle 通过，
  resident growth 1,015,808 bytes。两份自动 DDS 完全相同且各 384 字节，
  参数 raw 各 44 字节。脚本语法、Python 编译、`git diff --check` 通过。
- 最终 app `build-macos-debug/bin/qrenderdoc.app` GUI SHA-256
  `5b2bc3aa95cf…`，内嵌库与 `build-macos-debug/lib/librenderdoc.dylib`
  同为 `8c05d8b03281…`；T32/T33 正式 capture SHA-256 分别为
  `9aa1432c8d9a…`、`4478197f9994…`。`QA_BATCH33-34.md` 给出准确路径和
  同轮 EID 验收步骤。L3 条件未触发：批内及定向旧场景已圈定间接 dispatch、
  GPU 参数与 descriptor 风险，且未进入发布/合并门槛。
- `QA_PENDING.md` 中 T32/T33 均为“自动通过，待用户 GUI L4”。
  PHASE33、PHASE34、BATCH33-34 保持开放；全部未提交改动保留。

## 2026-09-26 BATCH33-34 P33.1–P33.2 进行中

- 无需新对话或 compact，已从 `PHASE33.md` P33.1 开始。T32
  `Metal_Compute_Indirect_Dispatch` native 夹具已添加：CPU 在 shared buffer
  byte offset 16 放置 12 字节 threadgroup 参数 `2,2,1`，以 `4,4,1`
  threads-per-threadgroup 执行；原生纹理、buffer 写入和哨兵核对通过。
- compute indirect dispatch 的 bridge、chunk、replay/action/usage 已接通并编译。
  T32 capture/XML 已生成，XML 保留 buffer、offset 16 和 threadgroup size；
  CLI replay 一次通过。Replay API 的逐事件/数据断言、异常拒绝及 T33 尚未完成，
  不将 T32 记为自动通过。当前 `QA_PENDING.md` 已登记 T32 批末 L4。
- 已成功构建 demo 与最终 app/renderdoccmd。首次构建误用不存在的 `qrenderdoc`
  target，改为 `build-qrenderdoc`；新增 chunk 后同步 `metal_stringise.cpp`
  的 Max 静态断言，构建通过。下一步补 T32 Replay API 自动断言，随后进入
  P34.1。此前所有未提交改动保留。

## 2026-09-26 BATCH31-32 关闭，下一项 P33.1

- 用户已确认 T30 公共事件树最短复验符合预期：EID 4/13 Begin/End 顶层，
  `$action()` 保留边界并隐藏 EID 8/11 状态调用。T30 原功能 L4 此前已通过；
  T31 的 EID 7 分组、批量绑定/空槽、筛选、画面及状态栏也已通过。
  T31 GUI 导出入口由用户明确免除本轮复验，自动 DDS/raw 数据已核对，
  不记作本轮 GUI 实测。`QA_PENDING.md` 当前无待验项。
- 最终构建上的联合定向证据沿用 `/tmp/batch31-32-scope-fix-final.log`：
  T01/T06/T07/T08/T10/T11/T20/T22/T23/T24/T25/T26/T27/T28/T29/T30/T31
  共 17 份 Replay API 与逐份 CLI replay、含 T00 的 18 份 capture × 10 轮
  lifecycle 通过。L3 条件未触发。此次只记录用户验收并编写下一批计划，
  未改代码或 captures，故不重复构建或测试；`git diff --check` 通过。
- PHASE31、PHASE32 与 BATCH31-32 已关闭，全部未提交改动保留。下一批
  `BATCH33-34.md`：T32 CPU 参数的 compute indirect dispatch，T33 GPU 生成
  参数的 compute indirect dispatch；第一项 `PHASE33.md` P33.1 fixture/native。

## 2026-09-26 T31 GUI L4 通过，导出入口本轮免复验

- 用户在 EID 18 的独立 Pipeline State 确认 sampler 0/1/3/4、Read-Only
  Textures 0/2/4、Read-Only Buffers 0/1/2/3/5/7 为 Empty，覆盖要求的
  sampler 3、texture 2、buffer 5。`$action()` 隐藏状态调用，清空筛选后恢复；
  状态栏为 `No problems detected`。结合此前对 EID 7、12–14、18 及后续
  绑定和画面的明确反馈，T31 GUI L4 记为通过。
- 用户明确表示本轮无需重复 GUI 导出入口测试。此前自动 DDS/raw 数据已核对；
  GUI 导出入口记录为用户免复验，不记为本轮实测。无需为此重新构建或回归。
  当前仅 T30 公共事件树最短复验待确认；PHASE31 与 BATCH31-32 仍不关闭，
  PHASE32 的 T31 L4 已通过，但随批次保持开放。全部未提交改动保留。

## 2026-09-26 T31 部分 L4 反馈与验收步骤修订

- 用户确认 T31 EID 7 层级、Texture 零值、Buffer 哨兵，EID 12/13/14 的
  `range` 以 `location, length` 显示正确，EID 18 及后续绑定和画面符合预期。
  T31 的虚拟 `Copy/Clear Pass #1` 消失是正常结果：通用 `AddFakeMarkers()`
  只在足够多的连续可归组 action 上生成 marker；修复后 Begin Compute 是边界，
  前方 blit action 不足以独立成组。Vulkan 使用相同通用规则。
- 原验收单将 API Inspector 与独立 Pipeline State 混写，已改为在 EID 18
  经 **Window → Pipeline State → CS → Show Empty Items** 一次检查
  texture 2、sampler 3、buffer 5 的空槽。代码和正式 captures 未变。
  `QA_PENDING.md` 保留 T30 事件树最短复验，以及 T31 三空槽、筛选、导出/
  保存与状态栏等未明确确认项；阶段和批次不关闭。

## 2026-09-26 T31 反馈修复与 Vulkan render pass 行为对齐

- 用户确认 T30 全部通过。T31 用户指出 EID 7 `Begin Metal Compute Pass` 被 GUI
  自动 `1–7 Copy/Clear Pass #1` 错收，且原验收单把资源初值写成其 Output。
  根因是 `ReplayController::AddFakeMarkers()` 将 `PassBoundary` 当作普通零输出
  action 合并；已令所有显式 pass 边界保留在自动分组外。T31 的 EID 7 Begin、
  EID 23 End 均为顶层，原 EID 保持不变；EID 7 右侧 Output 本来就应为空，
  Texture 20/21 的零值须在 Texture Viewer 手动选资源查看。
- 对照 Vulkan：Begin/End render pass 是 `PassBoundary`，`$action()` 保留；
  Vulkan 名称概括 attachment load/store，并记录 Clear/Discard/Resolve usage。
  Metal render pass 现在显示各 color/depth/stencil attachment 的 Load/Clear/
  Don't Care 与 Store/Resolve，记录 begin/end 对应 usage；compute pass 因无
  attachment 仍只标 Begin/End。pass boundary 不消耗 action 编号，与 Vulkan 一致。
- 新断言先在旧库重现 T31 假 marker 吞入边界，修复后通过；T07 深度模板与 T08
  MSAA resolve 的名称/usage 定向断言通过。最终库 SHA-256 `7a494e202d1b…`，
  GUI 程序 SHA-256 `d80297e329c9…`；T30/T31 正式 capture 未变，SHA-256 仍为
  `8bfe33501fad…` / `f5ad1c86660b…`。T01/T06/T07/T08/T10/T11/T20/T22/T23/
  T24/T25/T26/T27/T28/T29/T30/T31 共 17 份 Replay API 与逐份 CLI replay
  通过；含 T00 的 18 份 capture × 10 次 lifecycle 通过，resident growth
  1,392,640 bytes。`git diff --check` 通过。新日志
  `/tmp/batch31-32-scope-fix-final.log`；L3 条件仍未触发。
- 修订后的 `QA_BATCH31-32.md` 给出 T31 最短复验。T30 原功能 L4 已通过，
  但公共事件树改变需用户仅复验 Begin/End 层级与 `$action()`；T31 其他 GUI
  项仍待明确确认。PHASE32/BATCH31-32 不关闭；所有未提交改动保留。

## 2026-09-26 BATCH31-32 P31.1–P32.4 自动验证完成，用户 L4 待验

- T30 compute sampler 直接绑定与 T31 compute texture/sampler/buffer 批量绑定已实现；
  T31 包含非零 range、null 清除、已声明未使用绑定和后续直接覆盖。
  正式 capture 为 `captures/metal-smoke/t30_capture.rdc`、`t31_capture.rdc`；
  SHA-256 分别为 `8bfe33501fad…`、`f5ad1c86660b…`。最终 qrenderdoc
  `build-macos-debug/bin/qrenderdoc.app` 的程序 SHA-256 `d80297e329c9…`，
  内嵌与构建库均为 `fa401fb7e602…`。
- 最终定向证据：`/tmp/batch31-32-targeted.log` 中 T01/T03/T11/T12/T16/T17/
  T18/T19/T28/T29/T30/T31 的 native、Replay API、逐份 CLI 均通过。
  T30/T31 的正式 XML 与 Point/Linear 像素、状态快照、descriptor/usage、seek、
  DDS 和 T31 raw bytes 已核对；异常捕获 2+9 例及 T28/T29 的 10 例均拒绝。
  13 份 capture × 10 轮 lifecycle 通过，resident growth 1,638,400 bytes，
  日志 `/tmp/batch31-32-targeted.log`。脚本语法、Python 编译及 `git diff --check`
  通过。批次脚本已扩展 T30/T31，未跑全量 L3。
- L3 条件未触发：T12 已覆盖通用 descriptor 改动；无 render pass、初始资源内容、
  ICB 或公共事件层级改动，也未进入发布/合并门槛。
- 合并 GUI 验收单 `QA_BATCH31-32.md` 待用户同轮 qrenderdoc 检查。
  `QA_PENDING.md` 保留 T30/T31；反馈前 PHASE31/PHASE32/BATCH31-32 不关闭。
  全部工作区改动保留，下一项是接收 L4 后仅对不符项修复或复验。

## 2026-09-25 BATCH31-32 P31.1 开始

- 从 T30 compute sampler fixture/native 开始；基于 T11 的确定性 8×8 输入，
  增加 compute sampler 直接绑定与可区分的采样输出。`metal_compute_sampler.cpp`
  已加入 demo target，功能实现和验证进行中。当前工作区的新改动保留。
- `QA_PENDING.md` 已登记 T30 批末用户 L4；T31 尚未开始。前批 T28/T29
  已关闭且无遗留人工 QA。下一安全操作是完成 T30 未注入 native 参考验证，
  随后接通 capture/replay，再继续 T31。

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

最近关闭批次为 `BATCH33-34.md`；当前 `BATCH35-37.md` 的自动验证已完成，
T34/T35/T36 GUI L4 待后续 chat，因此批次保持开放。以本文件顶部 2026-09-26
检查点为准。BATCH29-32 也已关闭。
以下条目为 BATCH29-30 进行时的历史状态。

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
  含 T00 的 11 份 capture × 10 轮 lifecycle resident growth 1,638,400 bytes；脚本语法、
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
  11×10 lifecycle 全部通过，resident growth 1,638,400 bytes。同一最新 qrenderdoc 进程
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
