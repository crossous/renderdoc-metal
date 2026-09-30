# BATCH325：23:15 WindowServer watchdog 重启只读调查

2026-09-30 在远程 M4 Max / macOS 26.6 (25G72) 上只读检查诊断文件；未运行
UE、qrenderdoc、Metal GPU 夹具或回放。工作树仍为 `renderdoc-metal-t312`，
基础 HEAD `a7dd2bdae28adaa5a0a623663ea2557d4af0a600`；未提交或推送。
原 `renderdoc-metal` 工作树的四项修改未触碰。

## 可确认事实

- `/Library/Logs/DiagnosticReports/panic-full-2026-09-29-231538.0002.panic`
  记录 2026-09-29 23:15:38 的 `userspace watchdog timeout`：WindowServer
  连续 120 秒未成功 check in，watchdog 已诱发两次 WindowServer 崩溃。
  `ResetCounter-2026-09-29-231540.diag` 记录 `wdog,reset_in_1`。不是普通应用的进程崩溃，也不是新的
  `IOGPUResource::free` panic。
- WindowServer 在 23:05:34、23:09:50、23:09:56、23:11:16、23:11:23
  等时点多次短暂恢复显示就绪；23:10:07 和 23:12:24 报告显示卡住。
  `WindowServer-2026-09-29-231258.ips`、`...231348.ips`、
  `...231428.ips` 分别是 watchdog 终止报告。最后两份对应新 PID 19881，
  表明 WindowServer 被重启后仍无法恢复。
- `WindowServer_2026-09-29-231349_...userspace_watchdog_timeout.spin`
  和 `...231429...spin` 的主线程在全部 12 个采样里停于
  `QuartzCore → Metal → IOGPU → IOGPUFamily → AGXG16X` 命令缓冲提交；
  后者记录主线程约 78 秒未运行。证据直接指向图形提交链路不响应，
  **不能**仅凭栈确定是用户态回放参数、Metal 驱动还是硬件哪一层的缺陷。
- 23:12:48 的 stackshot 仍列出 PID 18848、19373 两个
  `ue_capture_open_probe`，均标为 `suspended (zombie)`，各有约 7.48 GiB
  footprint 记账，线程停在 `IOGPUFamily/AGXG16X` 内核路径。两者分别是
  BATCH324 中用户的一次与助手的 18 秒诊断；这些进程先前终止信号后
  未正常消失。Zombie 的 footprint 是快照记账值，不能据此声称当时仍
  占用 15 GiB 可用内存；panic 的 `memoryPressure` 为 false。
- 较早的 20:43:38 panic 也是 WindowServer 120 秒 watchdog，但其
  快照不含这两个探针。因此本次回放与 GPU 堵塞有强时间关联，不能
  认定它是所有 watchdog 的唯一根因或某个具体 Metal chunk 已被定位。

## 安全结论

BATCH324 的 `timeout` 和 RSS/footprint 监视只能限制用户态探针的发起
与观测，不能保证被终止的 GPU 提交立即退出内核路径。**不要在此远程机
再次对 UE 真帧、其 view 顺序诊断副本或同等级大帧执行 Metal GPU 回放，
也不要用 GUI 打开这些帧。** 不通过加长 timeout、重复 SIGKILL、删除
`waitUntilCompleted` 或忽略 `PurgeableStateEmpty` 试错。后续可以在本机
做只读 XML/代码审计与编译；需要 GPU 实证时转到有可靠本地重启途径的
另一台 Mac，从极小原生 Validation 夹具开始，记录 command buffer
身份、状态、错误、完成时间与资源 footprint，然后再判断完整功能族修复。

本批无代码修复或构建；无新的终端 GPU 通过、全量回归或人工 UI 验收；
累计成功 UI QA 增量 0。
