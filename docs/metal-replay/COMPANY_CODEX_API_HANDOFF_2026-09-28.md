# 公司 Codex API 临时接手单（2026-09-28）

> 2026-09-30 本地 Mac 接手时，直接使用
> [本地 Mac agent prompt](LOCAL_MAC_AGENT_HANDOFF_2026-09-30.md)；
> 以下为原始公司 API 交接与历史信息。

> 2026-09-30 M9 / BATCH325：先读 [23:15 WindowServer watchdog 调查](BATCH325_WINDOWSERVER_WATCHDOG_2026-09-29.md)。
> 本远程机已再次因 WindowServer/Metal/IOGPU 提交链路不响应而重启；
> 前一轮超时的 UE 探针仍见于重启前 GPU 内核栈。超时和 SIGKILL 无法
> 保证 GPU 工作安全退出。此机只做 CPU 审计与编译，停止 UE 大帧 GPU/GUI
> 回放；不能把时间关联当作具体 RenderDoc chunk 的根因证明。

> 2026-09-29 M9 / BATCH324：先读 [UE 真帧 GPU 等待与停测](BATCH324_UE58_GPU_WAIT_AND_REMOTE_HOST_STOP.md)。
> 同一诊断副本的 API-only 回放已进入 Metal `waitUntilCompleted`，采样的
> 物理 footprint 8.3 GiB，WindowServer 短暂不就绪；旧脚本的 RSS 限额
> 不覆盖该 footprint，超时后 SIGKILL 被拒。此远程机暂停 UE 大帧 GPU
> 回放。扫描告警和脚本退出记录已修、终端库已构建，**新库未在 UE 帧上运行**。

> 2026-09-29 M9 / BATCH323：先读 [帧尾 Empty 定向证据](BATCH323_TERMINAL_PURGEABLE_REPLAY.md)。
> 当前 Mac 的 4 KiB 原生、注入、API/CLI 与 GPU 字节/seek 小夹具通过；
> 原始 `UE58_capture.rdc` 仍有 15 个旧 view 顺序错误；诊断副本 SHA
> `45db2707…` 与 BATCH321 相同，仍未用本批库做 GPU 回放或 UI 验收。只接通有
> 后续无引用证明的帧尾 buffer `Empty`；不能外推到纹理、Volatile 或
> 帧中复用。下一步由用户对诊断副本做一次有界 API-only 回放并回传日志。

> 2026-09-29 M9 / BATCH322：先读 [CPU 审计与完成探针](BATCH322_UE58_CPU_AUDIT_AND_COMPLETION_PROBE.md)。
> 当前真帧已可只读 XML 导出，含场景/Nanite scope 和 10 个 MRT pass；
> 实际 GPU 回放仍安全拒绝。原生 completion/Empty 探针只编译未运行，
> 本远程 M4 Max 继续暂停 GPU 验证。

> 2026-09-29 M9：先读 [BATCH321](BATCH321_UE58_CAPTURE_VIEW_ORDER_AND_WATCHDOG.md)。
> 当前远程 M4 Max 发生 WindowServer watchdog 重启，已暂停本机 GPU 回放。
> `UE58_capture.rdc` 的 buffer view 创建顺序修正已写入代码，但旧帧推进到
> purgeable/GPU 完成边界后仍失败；当前只实现执行前安全拒绝，不能宣称
> 当前帧可正常开启。继续时先在其他机器完成该功能族闭环。

> 2026-09-29 M8：先读 [BATCH320](BATCH320.md) 与 STATUS 顶部。
> 用户已人工打开 `frame1770`，内容验收失败：仅 Slate pass 且 RT 全黑。
> 新 viewer 终端可显示 UE debug group；受控 viewport 按钮已编译、待用户
> 点击验证。Shader Converter GPU VA 仍是明确缺口。

> **后续 agent 先读：**遇到新 Metal 缺口，先按
> [跨 API 横向排查顺序](CROSS_API_TRIAGE.md)核对 RenderDoc 的 D3D12/Vulkan
> 实现与引擎源码，再处理 Metal 特有差异。本文的 BATCH311–312 停点已是
> 历史状态；本机 UE 最新进展见 [STATUS](STATUS.md) 顶部。

## 接手范围与停点

用户将本机当前 chat 暂停，待 Pro 额度重置后返回集中人工 QA；公司 Codex API 可继续
**终端可验证的 Metal capture/replay 开发**。不要把本单视为全量验收。当前停点是
[BATCH311–312](BATCH311-312.md)：Metal capture 格式版本 0xC，同步 tile/mesh
pipeline 的 Binary Archive 依赖与 `FailOnBinaryArchiveMiss` 已闭环。T301/T302 的
0xA、T309/T310 的 0xB 截帧仍可回放。先读 [STATUS](STATUS.md) 顶部、
[PLAN](PLAN.md) 当前节奏、[黑盒与稳定性门槛](BLACKBOX_GATE.md)，不要通读
HANDOFF 的历史条目。真实应用优先级见 [UE/Unity 路线](REAL_WORLD_CAPTURE_ROADMAP.md)。

原工作目录 `/Users/crossous/Developer/renderdoc-metal` 在收口时有约 510 个 Git status
条目，属于连续开发结果。移交时应取得 `crossous/renderdoc-metal` 的
`metal-replay-v1.46` 分支最新状态，或包含全部未提交、未跟踪文件的工作区快照；
不要仅使用旧基点 `45cc30721`。若另有未提交改动，不得重置或覆盖。本 chat
未启动 GUI/Computer Use；构建产物不会作为源码交接，须在目标机器重新构建。

## 已有证据（不是全量回归）

- 最近 32 份跨族截帧的 Metal Validation API + CLI 定向回放通过；命令：
  `bash util/buildscripts/scripts/test_metal_replay_targeted_macos.sh t01 t35 t48 t74 t75 t76 t77 t81 t82 t88 t89 t104 t115 t119 t128 t129 t130 t135 t144 t300 t301 t302 t303 t304 t305 t306 t307 t308 t309 t310 t311 t312`。
  日志目录 `build-macos-debug/metal-targeted.lTZRdG`。
- T301/T302/T305–T312 本族 315 例畸形输入均安全拒绝；T35/T305–T312
  共 9 份截帧各 10 次生命周期打开通过，驻留内存增长 638976 字节。T311/T312
  原生 Metal、注入截帧、GPU 输出和回放检查详见批次记录。全量累计/长时压力回归**未跑**。
- `build-qrenderdoc` 构建通过，但 GUI **从未启动验收**。当前 replay dylib 与
  app 内嵌 dylib SHA256 均为
  `2f88058307f598631a3be9ad0df7938b0486397bf0788d626b7ddc72cceb237f`；
  GUI 可执行文件为 `fe8bcf852b683bc463a3be883e6c54208b7bd45054a24f5dbace58c912346d74`。
  Qt 对所用新 SDK 有兼容性警告，实际 UI 行为待验。
- `captures/metal-smoke/t311_capture.rdc` SHA256
  `96acccaa0e498e4778e886f66f2ff01fbd3ed0c6bb6ad7154f031868c359851f`；
  `t312_capture.rdc` SHA256
  `2beb183fccd609cad533a1e126e6c3444d91c33b305076d83076e5193c801d07`。
- 最后收口的 `git diff --check`、集中脚本 `bash -n`、三个新 Python 负例脚本
  `py_compile` 均通过。原始宏文本匹配：bridge 59、chunk 15（后者含宏定义；
  前者含防御性拒绝），**不能**当作剩余 API 数量或完成比例。

## 必须保留的未完成事项

1. 2026-09-28 06:26:38 有一次 `IOGPUResource::free` 内核 panic；日志在
   `/Library/Logs/DiagnosticReports/Retired/panic-full-2026-09-28-062638.0002.panic`。
   当时有 `renderdoccmd`，但无法证明是本项目测试还是驱动缺陷。此后短测未见新增
   panic；避免直接运行累计长时 GPU 压测。要恢复全量门禁，请按
   [渐进复测规则](BLACKBOX_GATE.md#0626-panic-与渐进复测)逐级扩大并及时检查新报告。
2. Archive 的异步 tile/mesh pipeline 绑定、object-stage mesh archive、复杂
   stitched graph、intersection descriptor 仍明确拒绝；不能删除守卫后宣称支持。
   T61 的 GPU 生成 ICB range 也仍需 GPU 执行时序证据，见
   [PLAN 中的实施门槛](PLAN.md)。
3. [QA_CONSOLIDATED](QA_CONSOLIDATED.md) 与 [QA_PENDING](QA_PENDING.md) 记录
   274 份待人工 L4 的 capture。用户会在额度重置后集中 QA；当前应继续保持终端优先，
   不代替用户启动 UI。T311/T312 的重点是 archive→pipeline 资源图、options=4、
   tile 三阶段和 mesh 间接绘制的正反向 seek。
4. 尚无 UE 5.6.1 普通帧注入/捕获/回放证据。受控样例再多也不能替代该里程碑；
   可按真实首帧失败点决定下一个功能族，不必追逐宏计数。M4 专属路径要在目标机器验证。

## 公司端建议的最小续跑方式

先确认源码、capture、构建产物齐全和现有 Git 改动，再对将修改的功能族用
`test_metal_replay_targeted_macos.sh` 指定少量相关旧帧及新帧；每个新增入口保持
原生/捕获/API/CLI、畸形输入和资源生命周期证据。批内用定向测试，稳定性门禁后才
扩至全量脚本。新日志与新批次只追加简短状态和 QA 增量；不要把“定向通过”写成
“全量通过”或“人工 GUI 通过”。若公司端做了新改动，返回当前 chat 时请交接
提交/补丁位置、构建 hash、测试命令/结果、未跑项目与首个未解决阻塞点。
