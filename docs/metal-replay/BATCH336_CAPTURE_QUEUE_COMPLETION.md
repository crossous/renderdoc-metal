# BATCH336：应用队列完成边界、enqueue 顺序及真实 UE 帧首字节

2026-10-01，本地 M2 Pro / 16 GiB，HEAD c4be68bb…，保留全部未提交改动。
目标仍 active；未提交、推送或运行完整 UE GPU replay。

## 通用同步方案及 Metal 差异

参照 D3D12 StartFrameCapture 的 DeviceWaitForIdle、Vulkan 全应用队列等待：
Metal 在 PrepareInitialContents 前必须等待应用已提交的工作。原 WaitForGPU
仅使用 RenderDoc 内部队列，Shared 可见内存也不意味着 GPU 完成。

本批保留各应用队列实际提交的全部未完成 native command buffer；capture
transition 写锁划定提交边界，开始前等待最终 Completed/Error 状态。不能只
等每队列最后提交项：enqueue 的保留顺序可能与 commit 调用顺序不同。

Apple waitUntilCompleted 还包括 CPU completion handlers；帧首等待持有转换
写锁时，handler 可能调用 annotation 需要读锁，所以这里只等 GPU 状态。
帧末仍等完整 completion，但先取得 owned native 引用，再释放 tracking 锁，
允许应用完成回调继续 enqueue/commit。

- 每队列独立维护 enqueue 顺序，不再由一个全局列表阻塞其他队列。
- native enqueue/commit 与记录顺序在同一 tracking 锁内更新。
- captureCommitEpoch 防止背景已提交项因延迟解除 reservation 被归入新帧。
- Start 若存在已 commit 项被同队列未 commit reservation 阻塞，立即拒绝；
  End 若存在本帧同类未提交记录，丢弃不完整帧，不等待未来 commit。
- replay enqueue 执行 native enqueue；原先对尚未 commit 项调用
  waitUntilCompleted 不符合 Metal 队列语义。
- 自动 present 的 Start/End 在 commit 读锁释放后执行，保存 record、layer、
  native/proxy texture 和 drawable 引用，避免读锁升级死锁及生命周期丢失。

Apple 官方语义：[waitUntilCompleted](https://developer.apple.com/documentation/metal/mtlcommandbuffer/waituntilcompleted%28%29?language=objc)、
[status](https://developer.apple.com/documentation/metal/mtlcommandbuffer/status?language=objc)。

## 定向终端

运行 `bash util/buildscripts/scripts/test_metal_capture_initial_wait_macos.sh`。
日志 `build-macos-debug/metal-initial-wait.DC9jNi`、顶层 initial-wait-final.log。

- 两队列先 GPU 写 Shared 111/222，通过 CPU gate 延迟 100 ms；Start 等约
  104–110 ms，帧首实际字节及 API replay/四次 event seek 全部为 111/222。
- 完成回调调用需要 capture 读锁的 annotation，能正常返回。
- 帧末 callback 等 CPU gate 后 enqueue/commit 另一个队列，End 正常完成。
- 独立队列 uncommitted reservation 不阻塞 Start；同队列 blocked commit
  在 Start 和 End 分别立即拒绝，并能在补交 reservation 后恢复。
- 自动 TriggerCapture→present Start/End 保存成功；API/CLI replay 成功。
- CPU XML 证明背景 compute dispatch 未进入 frame stream；2×2清屏数据
  可读取，未以背景命令重复执行冒充初始状态。

库 SHA256 `12dd80aae4498524664b3fdb7fadbb9930dc31e26200b8948874a2866cd5d7a9`。
旧 t01/t02/t09/t11/t12/t35 API+CLI 通过，日志 metal-targeted.Rr5KFT。
七类 typed/sourced 描述符、78 API+CLI 负例及冻结快照重新验证，结果见
`build-macos-debug/descriptor-cutoff-final.log`（metal-descriptors.5CrI88）。

## 真实 UE 自动重截与新增审计

隔离 provider 仍为 B334 的 provider-build-stages，原 Engine dylib 未替换。
会话 `Testproj/Saved/RenderDocMetalSessions/20261001-054840`，End=1，正常
Metal NewMap；捕获后 SIGTERM 本次 owned UE PID1035，launcher exit0。

捕获使用库 `6a36b110fc3d3092c36fb3271ce1146962c6c52e51380d1ec20a1edda0747a9f`
（后续只是进一步收紧 native/record 顺序及帧末 callback 锁）。文件
15,543,612 bytes，SHA256
`a08ef579f0978afb33fa87069fa1a5aeb2aecde68402cdd65ef8ad81aa41e51b`；
保护副本 `Saved/RenderDocMetalCaptures/UE58_NewMap_queue_cutoff_a08ef579.rdc`。
本地 ue-queue-cutoff.rdc/zip.xml/audit.json 均在 local-m2-descriptor-replay。

新增审计逐字节比对 frame_start 活槽最新声明与 Initial Contents：
旧 c47bb56d 帧 1,466 match / 10 mismatch，全部差异来自最后的 GPUExpected。
新 a08ef579 帧 **1,476/1,476 match，0 mismatch**；lifetime/frame value/inline
issues 均 0。191 compute +584 vertex +285 fragment inline setter，2,253
非零 inline fields 对上显式 source；646 frame slot fields 对上 source。
4,188 历史字段源身份不保留仍作历史诊断，不能视为运行时别名或 shader 使用证明。

## 尚未完成的验收

全量回归未跑；本批人工 UI 未验，本机锁屏，旧成功 UI 不升级为本批验收。
完整 UE 正确画面/MRT/pass scope 尚未完成。UE provider 不声明完整 coverage，
未将 6 GiB heap、大帧及未支持的 GPU更新/alias/render 来源交给 GPU replay。
下一项继续扩展 sourced slot 的 GPU 更新时序及实际 shader 字节验证。
