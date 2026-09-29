# UE 5.8.3：按钮截帧结束时 command buffer 生命周期（2026-09-29）

## 用户会话与第一个失败点

- 独立 worktree `/Users/kurogames/Documents/Unreal Projects/renderdoc-metal-t312`，
  基础 HEAD `e0a26f7e65e22a3890aa38a318bde51d24babf6b`。原
  `renderdoc-metal` 的 4 项 Git 改动未碰；本批未提交、未推送。
- 用户会话 `SocoTestProj/Saved/RenderDocMetalSessions/20260929-024537/`
  的 manifest 确认加载的是本 worktree dylib，旧 SHA256
  `d2fa62f10f344df05337c34d70e8c1e79580e3c48e1732f03771b3429d3b1eea`。
  UE 日志 18:46:11.723 记录一次按钮请求；18:46:12.366 SIGABRT，
  堆栈落在 `WrappedMTLDevice::EndFrameCapture`。stdout 的最早异常为
  `-[AGXG16XFamilyBuffer waitUntilCompleted]: unrecognized selector`。
  18:48:39 UE 错误处理再次触发 EOS `IsInGameThread()` 断言，这是初始
  RenderDoc 异常后的次生错误，解释了长时间卡住才退出。
- 本次没有新 `.rdc`；项目 capture 目录最后仍是此前
  `UE58_frame99.rdc`。没有观察到新的内核 panic 报告。

## 横向核对与修复

按 [跨 API 排查顺序](CROSS_API_TRIAGE.md)检查帧结束：D3D12 的
`WrappedID3D12Device::EndFrameCapture` 在过渡到空闲时等待设备队列，
Vulkan 使用已记录 queue 及提交身份；两者均不依赖已失效的应用对象。
Metal 在 `m_CaptureCommandBuffersSubmitted` 中存 record，但原实现只在
`WrappedMTLCommandBuffer::commit()` 额外 retain 原生对象。UE 的短
autorelease pool 可在 `EndFrameCapture` 之前销毁内嵌 Objective-C 代理及
wrapper；随后通过 `record->m_Resource` 取 `Unwrap(commandBuffer)` 是悬空访问。
日志中的 AGX buffer 收到 command buffer 消息符合这一失效模式。

现在每条已提交的 record 同时保有代理本体和原生 command buffer；
帧结束直接等待 record 保有的原生对象，在序列化/后台提交清理点分别释放。
修复在 `metal_command_buffer.cpp`、`metal_core.cpp`、`metal_resources.h` 和
`metal_device.h`；短生命周期夹具扩展在
`util/test/demos/metal/metal_ue_compute_heaps.cpp`。这是 Metal 的
autorelease/代理映射问题，没有要求 UE 修改合法调用顺序。

## 定向终端结果

所有命令均在上述 worktree 执行；demo/API/CLI 子进程各有 30 秒超时：

```bash
cmake --build build-ue-debug --target renderdoc renderdoccmd -j 8
cmake --build build-metal-demos --target demos_x64 -j 8
MTL_DEBUG_LAYER=1 RENDERDOC_METAL_DRAIN_COMMAND_POOL_BEFORE_END=1 \
  bin/demos_x64 Metal_UE_Compute_Heaps --frames 5
MTL_DEBUG_LAYER=1 RENDERDOC_METAL_DRAIN_COMMAND_POOL_BEFORE_END=1 \
  RENDERDOC_METAL_CAPTURE_PATH=captures/metal-smoke/ue_command_pool_20260929 \
  DYLD_INSERT_LIBRARIES="$PWD/build-ue-debug/lib/librenderdoc.dylib" \
  bin/demos_x64 Metal_UE_Compute_Heaps --frames 5
MTL_DEBUG_LAYER=1 RENDERDOC_METAL_TRIGGER_CAPTURE=1 \
  RENDERDOC_METAL_DIRECT_PRESENT=1 \
  RENDERDOC_METAL_DRAIN_COMMAND_POOL_BEFORE_END=1 \
  RENDERDOC_METAL_CAPTURE_PATH=captures/metal-smoke/ue_button_command_pool_20260929 \
  DYLD_INSERT_LIBRARIES="$PWD/build-ue-debug/lib/librenderdoc.dylib" \
  bin/demos_x64 Metal_UE_Compute_Heaps --frames 5
MTL_DEBUG_LAYER=1 build-ue-debug/bin/renderdoccmd replay --loops 1 \
  captures/metal-smoke/ue_button_command_pool_20260929_frame3.rdc
MTL_DEBUG_LAYER=1 build-ue-debug/metal_replay_output_smoke \
  captures/metal-smoke/ue_button_command_pool_20260929_frame3.rdc \
  /tmp/ue-button-command-pool.ppm
```

- 两项构建通过；原生、显式截帧、按钮式 trigger/direct present 夹具均
  5 帧退出 0。两份新夹具截帧各单次 CLI/API 回放退出 0，Metal Validation
  已启用；旧 UE `UE58_frame99.rdc` 也单次 CLI 回放退出 0。
- 最终 dylib SHA256
  `1ab4448a93b2c8f08211087bb40f2afbf72a9231d98c978918ea5425fe3b6ef9`。
  短 pool 截帧 SHA256
  `861cfc3232e148e060d72461315b8cee2a5e7130165e964fc996439bb515bcb4`；
  按钮式截帧 SHA256
  `f916a29252c2522b2b0bd3dfad748184478419a68c0a54410b65e734c16c7818`。
  `git diff --check` 通过。
- 实施期间第一次短夹具暴露 `m_ObjcBridge` 是 isa 字段而非代理地址，
  对它调用 `retain` 曾导致测试进程 SIGSEGV；已改为 retain wrapper 起始地址，
  上述测试均使用修正后的最终 dylib。没有继续用错误构建运行 UE。

本批是**定向终端通过**。没有用新库启动 UE、没有新的 UE `.rdc`，
没有全量回归、长时 GPU 压测、GUI/Computer Use 或人工 UI 验收。
累计 UI QA 增量 **0**，原 274 份待验不变。第一个仍待确认的点是
**同一 UE 按钮截帧在新库下能否正常保存 `.rdc`**；若仍失败，记录新会话的
最早 API/bridge/chunk，再按跨 API 排查顺序处理。
