# 本地 M2 Pro 接手与 UE 5.8.3 重截入口

2026-09-30（Asia/Shanghai），工作树 `/Users/crossous/Developer/renderdoc-metal`。
更新前 HEAD `e0a26f7e65e22a3890aa38a318bde51d24babf6b`、工作树干净。
`git fetch renderdoc-metal` 后确认 GitHub 默认分支 `metal-replay-v1.46`，
`git merge --ff-only renderdoc-metal/metal-replay-v1.46` 快进至
`c4be68bb7fce662e4dd8498408981fa2e824c3b6`。未 reset/stash、未提交或推送。

## 环境及项目改动

- Apple M2 Pro / 19 GPU cores / 16 GiB，macOS 26.1 (25B5042k)，arm64。
- Xcode 26.0.1 (17A400)，系统报告 Metal 4；磁盘检查时剩余约 29 GiB。
- `/Users/Shared/Epic Games/UE_5.8/Engine/Build/Build.version`：5.8.3，CL 58210709。
- 项目 `/Users/crossous/Documents/Unreal Projects/Testproj/Testproj.uproject`。
  起始没有项目级 Plugins 目录或 `.rdc`；未找到远端 UE 原件。
- `.uproject` 修改前副本保存在
  `build-macos-debug/local-m2-handoff/Testproj.uproject.before-plugin`。
  用仓库安装器复制 `RenderDocMetalCapture` 和当前 app API header 到项目的
  `Plugins/RenderDocMetalCapture`，仅在 descriptor 追加 Enabled 项。
  未改 Engine 源码、项目 Config 或 Content。
- 本机启动快捷入口位于项目 `Saved/RenderDocMetalLocalM2.command`，
  设置 Testproj、当前 build、Entry 地图、15 FPS、50% screen percentage、
  `r.Nanite 0`。已在运行时不要再次双击启动。

## 静态对照与本次修改

读取 BATCH322–325、CROSS_API_TRIAGE、BLACKBOX_GATE 及当前 QA 总单。
本地源码对照：D3D12 `d3d12_device.cpp::QueueWaitForIdle` 在 queue signal 后
等待 fence；Vulkan `vk_core.cpp::FlushQ` 在 QueueWaitIdle 后执行 pending cleanup。
UE `MetalSubmission.cpp::ProcessInterruptQueue` 检查每个 work CB 和后继 signal
CB 的 Completed/Error 状态，再处理回收；`MetalTempAllocator.cpp` 的
DeferredDelete 闭包才把临时 buffer 设为 Empty。

当前 Metal `FinishReplayCommands` 先提交剩余 partial tails，再逐个等待已有
提交，最后执行 deferred terminal Empty；等待没有被移除。小正例不能说明
远端 64 个 CB 中哪一个导致 GPU 卡住。未拿到原件/XML，本机不能重做其完整
时序审计；仓库审计证据与远端的 12 heap / 6.062 GiB 记录仅作为交接证据。

本次没有修改 driver 或截帧格式，修改两项工具：

1. `audit_ue_metal_xml.py` 增加 heap 声明容量、所有 CB 创建/队列/encoder/
   提交记录、signal/wait 与 completion handler 注册。GPU 操作计数限 draw、
   dispatch、blit copy/fill，并显式报告无法归属的操作；不把 CPU chunk 顺序
   或 handler 注册当 GPU 完成证据，也不把 heap 容量当 footprint。
2. `run_ue_metal_capture_macos.sh` 把调用者参数放在 project 后、默认 flags 前。
   UE `UnrealEdMisc.cpp:397` 只读取首个 token 作为地图；旧顺序使 Entry 参数
   被忽略而加载 FirstPerson。第一次会话 `20260930-175623` 已发送 SIGTERM，
   UE 写出 GracefulTerminationHandler/Log file closed，PID 13037 随后消失；
   编辑运行中的脚本导致旧 shell 在子进程返回后报告语法错误，当前脚本的
   `bash -n` 和内嵌 Python 语法/参数顺序检查均通过。

## 构建与定向终端验证

日志目录：`build-macos-debug/local-m2-handoff/`。

- `cmake --build build-macos-debug --target renderdoc renderdoccmd -j 4`：成功，
  `renderdoc-build.log`。
- UE `Build.sh TestprojEditor Mac Development -Project=<Testproj.uproject>
  -MaxParallelActions=4`：Succeeded，`ue-plugin-build.log`。
- `cmake --build build-macos-debug --target build-qrenderdoc -j 4`：成功，
  `viewer-build.log`；Qt 对 SDK 26 仍有既有兼容性警告，未运行 viewer。
- 原生 `run_purgeable_completion_probe_macos.sh --run`：一次 Metal Validation
  4 KiB blit、后继 signal/callback、Empty、恢复及再次 blit 的 GPU 字节通过。
  `completion/run.log`。
- 新库注入同一 terminal Empty 小夹具：退出 0，`completion/injected.log`，
  新帧 `completion/t323_terminal_capture.rdc`（约 1.2 KiB）。
- 单次脚本，10 秒、1024 MiB、`RENDERDOC_METAL_TRACE_REPLAY_WAITS=1`：
  小帧 API/CLI 各退出 0，最高采样 footprint 2.4/33.2 MiB。
  `completion/api-cli.log` 指向 `rdm-ue-replay.i8veuQ` 的详细日志。
- 专用 `metal_purgeable_completion_replay`：4 KiB 全部 `0x5a`，两次事件
  seek 通过，`completion/bytes-seek.log`；等待结束状态均为 Completed (4)。
- `metal_purgeable_completion_invalid.py`：Empty 后重新引用负例在帧 GPU
  执行前拒绝，`completion/invalid.log`。
- `test_metal_replay_targeted_macos.sh t35 t62`：两帧 API/CLI 均通过，
  `targeted-t35-t62.log`，详细日志 `build-macos-debug/metal-targeted.IZN5QZ`。
  `metal_purgeable_state_invalid.py` 的 10 例旧负例通过，`t62-invalid.log`。
- 新 CPU audit 用小帧 XML 验证三个 CB 的身份/提交、signal、callback 注册及
  purge 前读引用；T117 XML 验证一个 65536-byte heap 与三次提交。输出为
  `completion/audit.json`、`t117-audit.json`；断言与 `git diff --check` 通过。

SHA256：

| 产物 | SHA256 |
| --- | --- |
| 终端库及 viewer 内嵌库（cmp 相同） | `f4f4e9a4cae5d99e77bbba26b18650599e5cf1f4a9d454e963b2163d540a37be` |
| renderdoccmd | `ffcebc3677ce0a39b00fc31fccd96b3e9324c6b02d801f9eade6e131719c4b72` |
| qrenderdoc 可执行文件 | `24d8b4a5f61d6050c69ad956e41a05d9a162057439deb62a96d11158d0c2c93f` |
| Testproj 插件 dylib | `9de352409d50c96ace22d60d47b31a43e03a330507d711805d364faf105e26fb` |
| 小 terminal Empty 帧 | `60861ca15f998dffd63103fa85d9fc0b928dbba1cfe7a18e9046750be135735d` |

## UE 重截及下一步

修正地图顺序后启动会话 `Saved/RenderDocMetalSessions/20260930-175932`。
启动 stdout 另存 `build-macos-debug/local-m2-handoff/ue-entry-launch.log`。
日志已确认 RenderDoc API resolved、Engine initialized、Entry MAP LOAD，及
15 FPS / 50% / Nanite off；没有新 `.rdc`。由用户在这个空地图点击一次
**Capture Metal Frame**，等待保存路径提示。
文件位于项目 `Saved/RenderDocMetalCaptures/`。首轮不进 PIE、不打开 FirstPerson。
空帧通过后才增添少量普通几何并比较原生与 replay。

取得新 `.rdc` 后先 hash、用 `renderdoccmd convert` 只读导出 XML，再运行
增强的 audit，检查 heap 容量、CB 提交、event、purge 和 scene pass；按结果
选择最小独立复现或一次小帧 API-only 回放。旧远端原件和 view 顺序诊断
副本均未在本机 GPU 打开。

**定向终端通过；全量回归未运行；人工 qrenderdoc UI 未验收；新 UE 帧尚未
取得，UE 正确画面/资源/scope/MRT 与完整 replay 仍未证明。**
QA_PENDING 的所有旧待验项继续保留；本次不关闭任何旧 batch。
