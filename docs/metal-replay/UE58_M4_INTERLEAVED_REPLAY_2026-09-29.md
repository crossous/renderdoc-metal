# UE 5.8.3 / M4 Max：交错 command buffer 定向回放（2026-09-29）

## 工作树与版本

- 独立工作树：`/Users/kurogames/Documents/Unreal Projects/renderdoc-metal-t312`；
  基础 HEAD `e0a26f7e65e22a3890aa38a318bde51d24babf6b`，来自
  `crossous/renderdoc-metal` 的 `metal-replay-v1.46`。本批改动未提交、未推送。
  原 `/Users/kurogames/Documents/Unreal Projects/renderdoc-metal` 的 4 项
  Git 改动未 reset、stash 或清理。
- 本机 macOS 26.6（25G72）、Apple M4 Max（40 GPU 核）；Epic UE 5.8.3
  CL 58210709；项目为 `SocoTestProj`。当前项目尚未固定为严格的非 Nanite
  最小场景。
- 复用前轮实际注入 UE 取得的
  `SocoTestProj/Saved/RenderDocMetalCaptures/UE58_frame99.rdc`，SHA256
  `8a55f0e3738d8b10255dc855e13933ef6e9ab1055f7f59831fe3cfdba4a51022`。
  前轮 `20260929-010300` session 的插件日志记录 `Capture saved`；本批没有
  再启动 UE 或生成新 UE 截帧。
- 当前 `build-ue-debug/lib/librenderdoc.dylib` SHA256
  `d2fa62f10f344df05337c34d70e8c1e79580e3c48e1732f03771b3429d3b1eea`。
  交错夹具 `captures/metal-smoke/ue_interleaved_command_buffers_capture_capture.rdc`
  SHA256 `276c3c64ef89204c517d5333e4f1c3a544b5c6e98b90786eaa961ab414a583d3`。

## 横向排查与修复

先按 [跨 API 排查顺序](CROSS_API_TRIAGE.md)核对现成方案。RenderDoc 的
`renderdoc/driver/d3d12/d3d12_command_queue_wrap.cpp` 在
`Serialise_ExecuteCommandLists` 中以列表身份找到已录制命令，于执行点提交；
`renderdoc/driver/vulkan/wrappers/vk_queue_funcs.cpp` 在
`Serialise_vkQueueSubmit` 中以提交信息关联 command buffer。它们不因后来
创建了另一个命令对象就提前提交先前对象。UE 5.8 的多个 Metal command buffer
先创建、后回到较早对象编码属于原生 Metal 允许的顺序；UE 源码
`Engine/Source/Runtime/Apple/MetalRHI/Private/MetalCommandEncoder.cpp` 的
`StartCommandBuffer` 与 `MetalSubmission.cpp` 的 `FlushBatchedPayloads` 分开
创建、累计和提交 command buffer。对这些路径的源码搜索未见 RenderDoc
条件分支。原生夹具在 `MTL_DEBUG_LAYER=1` 下通过，因此没有证据表明这是
UE 为 RenderDoc 做的特殊适配。

原 Metal replay 在创建下一个 buffer 时通过 `FinishReplayCommands()` 结束并
提交上一个 buffer；UE 真帧随后回到较早 buffer 的 blit encoder，原先在
`setCurrentCommandEncoder:` 触发 Validation abort，加守卫后安全拒绝于
`MTLCommandBuffer::blitCommandEncoderWithDescriptor`。现在按 command buffer
身份保留 encoder、render target 与 replay 管线状态，在捕获的 commit chunk
提交，并于提交前应用该 buffer 对应的 Shared CPU 写入。render、compute、
blit 等 encoder 入口均切换到所属 buffer。另修正 compute encoder 创建归属
和 `active=false` 的 Metal 顶层反射参数校验。此次修复没有删去身份守卫。

主要改动位于 `renderdoc/driver/metal/metal_core.cpp`、
`metal_device.{cpp,h}`、`metal_command_buffer.{cpp,h}`、各 encoder 的
`cpp/h/bridge.mm`、`metal_replay.{cpp,h}`、`metal_buffer.cpp` 等；
交错夹具扩展在 `util/test/demos/metal/metal_ue_compute_heaps.cpp`，
API 探针在 `util/test/metal/metal_replay_output_smoke.mm`，两例对象归属
负例在 `util/test/metal/metal_ue_interleaved_command_buffers_gate.py`。
本工作树还有先前 UE blit/counter、heap、parallel 与接入脚本改动；
完整文件清单以 `git status --short` 为准。

## 定向终端结果

以下为从独立工作树调用的可复现命令；本轮 API/CLI 的实际调用由
`python3 subprocess.run(..., timeout=30)` 包裹，相同可执行文件、参数与环境，
每个进程最多 30 秒。前一轮完成交错夹具的原生 Metal Validation
两帧、注入五帧截帧和 API GPU 数据/seek 检查；本轮未重复生成该截帧。
本轮对最终 dylib 再执行一次增量构建、夹具 gate、API 与 CLI：

```bash
cmake --build build-ue-debug --target renderdoc renderdoccmd -j 8
MTL_DEBUG_LAYER=1 python3 util/test/metal/metal_ue_interleaved_command_buffers_gate.py \
  build-ue-debug/bin/renderdoccmd \
  captures/metal-smoke/ue_interleaved_command_buffers_capture_capture.rdc
MTL_DEBUG_LAYER=1 RENDERDOC_METAL_REQUIRE_INTERLEAVED=1 \
  build-ue-debug/metal_replay_output_smoke \
  captures/metal-smoke/ue_interleaved_command_buffers_capture_capture.rdc \
  /tmp/ue-interleaved-evidence.ppm
MTL_DEBUG_LAYER=1 RENDERDOC_METAL_UE_FRAME_PROBE=1 \
  build-ue-debug/metal_replay_output_smoke \
  '/Users/kurogames/Documents/Unreal Projects/SocoTestProj/Saved/RenderDocMetalCaptures/UE58_frame99.rdc' \
  /tmp/ue58-evidence.ppm
MTL_DEBUG_LAYER=1 build-ue-debug/bin/renderdoccmd replay --loops 1 \
  '/Users/kurogames/Documents/Unreal Projects/SocoTestProj/Saved/RenderDocMetalCaptures/UE58_frame99.rdc'
```

- 增量构建退出 0（`ninja: no work to do`）；夹具 gate 退出 0，CLI
  成功回放，2 例未知 encoder/commit owner 的畸形截帧安全拒绝。
- 交错夹具 API helper 退出 0；该 helper 检查 GPU 输出和前进/回跳 seek。
  `RENDERDOC_METAL_REQUIRE_INTERLEAVED=1` 还要求帧中确有先创建两个 buffer、
  再回到第一个编码，并要求两个 buffer 各有 commit 身份。
  UE 真帧 API 探针退出 0，报告 `forward/rewind draw seek passed, draws=165`：
  抽取首、四分之一、中间、四分之三、末尾及回跳首 draw，对照各 draw 的
  pipeline 资源身份和截帧 chunk 一致。它不是 UE 原生画面像素对照。
- UE 真帧 CLI 单次完整回放退出 0，`Metal API Validation Enabled`。
  同样以 `MTL_DEBUG_LAYER=1 ... renderdoccmd replay --loops 1` 对
  `t35_capture.rdc`、`t41_capture.rdc`、`t114_capture.rdc`、
  `t312_capture.rdc` 各单次回放，均退出 0。前一轮另已对 T56、T101 和
  UE parallel 等旧帧做定向回归；本轮没有扩大到全量。
- `git diff --check` 退出 0。构建及回放未报告新 GPU Validation 错误。

这些结果只表示**定向终端通过**。未运行：本批新 UE 注入截帧、严格
非 Nanite 最小场景、UE 原生画面与回放 GPU 输出逐项对照、全量回归、
长时 GPU 压测、GUI/Computer Use 与人工 UI 验收。累计 UI QA 增量 **0**；
原有 274 份待人工验收不变。

## 第一个仍未解决的阻塞点

这一份 UE 真帧已越过交错 command buffer 回放阻塞。下一个里程碑是把
`SocoTestProj` 固定为严格非 Nanite 最小场景，由用户用当前注入/按钮路径
再截一帧，逐项对照原生 UE 画面和回放 GPU 输出，再由用户检查 UI 事件树
及资源。当前没有可证明的新 API/bridge/chunk 阻塞点；不能据此宣称任意
UE 场景、Nanite、全量稳定性或 UI 已通过。若新帧遇到失败，依
[跨 API 排查顺序](CROSS_API_TRIAGE.md)定位最早调用后再选对应功能族。
