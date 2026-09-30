# BATCH324：UE 真帧 GPU 等待与远程机停测

2026-09-29，独立工作树 `/Users/kurogames/Documents/Unreal Projects/renderdoc-metal-t312`，
基础 HEAD `a7dd2bdae28adaa5a0a623663ea2557d4af0a600`；本批未提交或推送。
原 `renderdoc-metal` 目录的四项本地改动未触碰。

## 实测

用户对旧帧的 view 顺序诊断副本运行 API-only 回放，日志目录
`/var/folders/cm/wrylyc0x78vbwmff8py06ktm0000gn/T/rdm-ue-replay.MKSf8r`。
副本 SHA256 `45db270790407660288878daaace741f0209bb08183a2b0221324c2d813deb2d`；
当时库 SHA256 `27bdde246ee87f426920d004a1b5c1503bf71671f44bcfa2382a760b5de97e50`。
23:00:57 进入 OpenCapture，23:00:58 到达帧内扫描及 debug group 事件；
没有 `result.txt`，无法据此确定完成、失败或谁终止了脚本；没有对应新 crash report。

助手随后自己在**相同旧库**和诊断副本上做两次 API-only 有界定位：

```bash
RENDERDOC_METAL_BUILD_DIR=/private/tmp/rdm-t312-build \
UE_METAL_REPLAY_API_ONLY=1 UE_METAL_REPLAY_TIMEOUT_SECONDS=12 \
UE_METAL_REPLAY_MAX_RSS_MIB=1024 \
bash util/ue/replay_ue_metal_once_macos.sh \
  /private/tmp/rdm-t323/UE58_capture_view_ordered.rdc
```

第一次日志 `rdm-ue-replay.hWoNlu`：API 在 12 秒超时后退出 -15，
脚本采到的最高 RSS 76.7 MiB；只确认 `OpenFile OK`，没有 OpenCapture 结果。
第二次把超时改为 18 秒，日志 `rdm-ue-replay.QCLoxT`，并于运行中执行
`/usr/bin/sample 19373 3 -file .../api.sample.txt`。采样栈稳定落在
`WrappedMTLDevice::ReadLogInitialisation` → `FinishReplayCommands` →
`[_MTLCommandBuffer waitUntilCompleted]`，另一个线程在
`IOGPUCommandQueueSubmitCommandBuffers`。采样显示物理 footprint **8.3 GiB**；
脚本的 RSS 采样最高仅 923.3 MiB，不能作为统一内存 GPU 资源的安全上限。
18 秒后脚本发 SIGTERM/SIGKILL，等待未结束，SIGKILL 返回 EPERM；原脚本
抛异常，没写 `result.txt`。当时 PID 19373 在 `ps` 中呈 `?Es`、RSS 0，
已由 launchd 接管；不把它算作成功终止。系统日志于 23:05:34 记录
WindowServer 在 6.34 秒失去就绪后恢复。本机启动时间仍为 20:43:28，
本批未发现新 panic 或重启。**达到黑盒停测门槛，停止此机 UE 大帧 GPU 回放。**

## 静态对照及可确认边界

已有 CPU XML `/private/tmp/rdm-t322/UE58_capture.xml` 表明此帧有 12 个
Metal heap、声明大小合计 6.062 GiB、64 个 `MTLCommandBuffer::commit`、
17 个 `encodeSignalEvent`，没有 `encodeWaitForEvent` chunk。因此不能把等待
直接归咎于捕获的显式 event 死锁；也不能证明是某个 shader 或 purgeable
错误。RenderDoc D3D12/Vulkan 的提交后完成边界与 UE 的 DeferredDelete
已在 BATCH323 对照；本次新增的阻塞位于 Metal GPU 完成等待，须先有
具体 command buffer 状态与 GPU 完成/错误证据，再改提交或回收语义。
旧的 15 个 view 顺序错误仍使原始 RDC 不能直接测此问题。

两次帧预扫描在读取 chunk 的部分字段后又调用 `SkipCurrentChunk`，造成
`Partially consumed bytes` 警告；现改用 `EndChunk` 正常跳过剩余字段。
脚本新增进度文件，超时后即使 SIGKILL 被系统拒绝也记录
`kill_denied`/`still_running`，并在 API 探针开始 OpenCapture 前写边界日志。
脚本现调用 macOS `proc_pid_rusage` 监控 `ri_phys_footprint`，与 RSS 共用
原有 MiB 上限；如果连续五次无法读取 footprint，会停止测试。内嵌 Python
语法、本机自身进程的 `rusage_info_v0` 读数检查，以及三个无 GPU 子进程
定向运行通过：1 MiB RSS 门控、3 秒超时、16 MiB 内存进程门控都写出了
`result.txt` 和 `api.progress.txt`；日志位于
`/private/tmp/rdm-replay-watchdog.CbF7hR`、
`/private/tmp/rdm-replay-watchdog-footprint.EXUmu0`、
`/private/tmp/rdm-replay-watchdog-footprint-hit.dx7E1X`。这些测试没有制造
footprint 高于 RSS 的 Metal 负载，**不能证明本机大帧可安全终止**。
这些是诊断与日志修正，**不解决 GPU 等待，也没有用新库重跑 UE 真帧**。
`cmake --build /private/tmp/rdm-t312-build --target renderdoc renderdoccmd -j 4`、
`bash -n`、内嵌 Python 语法及 `git diff --check` 通过。新终端库 SHA256
`0830156a880fd2c9743544cea5ad44c6fe44b2fefa9528fbe5c424957a80b8db`，
CLI SHA256 `9ab6f5ac2ab1704d351ae1c84a4d43811646e1862e523b3d80f799e1c4b8b38c`。
qrenderdoc.app 的内嵌库未随本批重建或人工启动。

## 下一步与验收

先确保本机 WindowServer 与 PID 19373 已正常恢复；之后只做 CPU 审计、
小夹具或在另一台可承受 GPU 负载的 Mac 上验证。要恢复大帧测试，必须
先解决 GPU 完成等待与物理 footprint 监控；不能通过删除等待或忽略
`Empty` 冒充回放成功。待安全门槛满足，再用新库一次性验证 API/CLI 打开、
像素、MRT、scope 和 GPU VA。当前 UE 真帧仍未正常开启，黑 RT 未解决。

**本批终端 GPU 大帧测试未通过；全量回归未跑；人工 UI 未验收；
累计成功 UI QA 增量 0。**
