# 本地 Mac 接手 prompt（2026-09-30）

以下文字可直接发给本地 Mac 上的新 agent。仓库默认分支为
`metal-replay-v1.46`；开始前以 GitHub 实际 HEAD 和本机文件为准。

> 请接手 `https://github.com/crossous/renderdoc-metal` 的默认分支
> `metal-replay-v1.46`，继续 RenderDoc Metal 的 UE 5.8 截帧回放与稳定性
> 调查。先 `git fetch` 并记录 HEAD、工作树状态；保留你机器上已有改动，
> 需要隔离时新建 worktree，不要 reset、stash 或覆盖。不要与另一 agent
> 并发修改同一工作树；不要自动提交或推送，除非我另行要求。
>
> 先读 `docs/metal-replay/STATUS.md` 顶部、`PLAN.md` 当前优先级、
> `CROSS_API_TRIAGE.md`、`BLACKBOX_GATE.md`、
> `BATCH322_UE58_CPU_AUDIT_AND_COMPLETION_PROBE.md` 至
> `BATCH325_WINDOWSERVER_WATCHDOG_2026-09-29.md`，以及
> `QA_CONSOLIDATED.md` 顶部。遇到通用图形 API 问题，先横向查
> RenderDoc D3D12/Vulkan 和 UE 源码怎样保持资源身份、提交与完成边界；
> 只有 Metal 特有约束再设计新方案。不能靠删除守卫、忽略 Empty、跳过
> GPU 等待或原生透传宣称支持。
>
> 当前远程 M4 Max / macOS 26.6 在 2026-09-29 20:43 和 23:15 两次发生
> WindowServer watchdog 整机重启。后一次前，旧 UE 大帧的两个已终止
> `ue_capture_open_probe` 仍以 zombie 状态存在，约 7.48 GiB footprint
> 记账且线程在 IOGPU/AGXG16X；WindowServer 主线程也卡在 Metal GPU
> 提交。时间关联很强，但还未证明 RenderDoc 是唯一根因。该远程 Mac
> 已停做 UE 大帧 GPU/GUI 测试。不要让它再次回放这张帧。`timeout`、
> RSS/footprint 上限和 SIGKILL 不能保证卡在内核的 GPU 工作退出。
>
> 当前代码的 BATCH323 帧尾 buffer `PurgeableStateEmpty` 只在 4 KiB
> 原生 Metal Validation、注入截帧、API/CLI、GPU 字节/seek 及相关负例
> 与旧帧的**定向终端测试**中通过。BATCH324 修了帧扫描告警与脚本日志，
> 终端库编译通过；这版没有在 UE 真帧上验证。全量压力回归未运行，
> 人工 UI 验收没有新增通过项。不要把这些小夹具结果写成 UE 回放成功。
>
> 原始 UE 文件 `UE58_capture.rdc` 的 SHA256 是
> `a96e685f726608bb30a84f2ae0c7485d2edaaad82553b8e95da1df44e91915f0`，
> 位于远程机的
> `~/Documents/Unreal Projects/SocoTestProj/Saved/RenderDocMetalCaptures/`，
> **不在 GitHub 仓库内**。它有 15 个旧的 buffer texture view 创建顺序
> 错误；`util/ue/repair_ue_metal_buffer_view_order.py` 可生成只用于诊断的
> 副本，已知副本 SHA256
> `45db270790407660288878daaace741f0209bb08183a2b0221324c2d813deb2d`。
> 原件不可改。请先确认本地是否拿到原件及其哈希；没有就列为阻塞，
> 先做独立的 CPU 静态审计和小夹具，不要凭交接文字伪造真帧测试。
>
> 先记录本地 Mac 型号、GPU、RAM、系统、Xcode、UE 和项目是否存在，
> 以及 repo、库、截帧哈希。若本地机器内存低于远程 64 GiB，要特别注意
> 该 UE 帧在 XML 中声明了 12 个 heap，合计约 6.062 GiB；旧回放采样
> footprint 曾达 8.3 GiB。不要直接在较低配置 Mac 上打开完整 UE 帧。
> 优先在无 GPU 的 XML/代码路径里审核 64 个 command buffer 的提交、
> GPU 完成和回收时序，找出可缩减到几 KiB 的原生 Metal 最小正例，
> 并对照 D3D12/Vulkan 的完成语义。GPU 实测只在有可靠本地重启能力、
> 独立日志和严格单次门槛后开展；任何新的 GPU 错误、异常资源增长或
> WindowServer 卡顿立即停止升级负载。
>
> 最终给我具体证据：工作树来源、改动文件、构建和截帧 SHA256、实际
> 命令与结果、没有运行的测试、首个仍未解决的阻塞点。明确区分
> “定向终端通过”“全量回归通过”“人工 UI 通过”。目前这张 UE 真帧
> 尚未正常开启，正确画面、MRT 和 pass scope 都未通过验收。

## 当前远程工作区额外说明

旧目录 `/Users/kurogames/Documents/Unreal Projects/renderdoc-metal` 仍有四项
预先存在的 Git 修改，保持原样；独立工作树
`/Users/kurogames/Documents/Unreal Projects/renderdoc-metal-t312` 承载本次
项目成果。原 `.rdc` 存在于 UE 项目 `Saved` 目录，因其不在仓库内，
迁移时需单独复制并核对 SHA256。`captures/metal-smoke` 的受控小截帧
已经在仓库中。BATCH324 使用的 `/private/tmp` 诊断副本在重启后已消失，
可由原件和修复脚本重新生成，但不要在远程机 GPU 回放它。
