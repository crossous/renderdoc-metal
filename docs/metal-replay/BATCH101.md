# BATCH101 — Stage-boundary counter sampling

接通 `newCounterSampleBufferWithDescriptor` bridge/旧 chunk1035，以及 blit
`resolveCounters` 旧 chunk1228。捕获格式 v5 记录 counter set、sample count、
Shared storage、render pass 的四个阶段索引与资源 ID。回放保留资源身份、pass
绑定、resolve Copy 事件及源/目标用量；对越界索引、错误资源类型、范围和偏移
拒绝。仅支持本机已验证的 `timestamp` + `AtStageBoundary`，不宣称
`AtBlitBoundary` 或 encoder 内主动 `sampleCountersInBuffer` 已接通。

T101 使用普通 vertex/fragment 三角形而非 mesh：M2 Pro 原生 probe 和测试
均取得非零递增的 vertex/fragment 起止四个时间戳。Render 与 resolve 使用
两次顺序提交；同一提交里多帧 resolve 曾偶发写零，因此该边界暂记为原生
行为限制，不作为回放正确性的证据。原生三轮×五帧、capture/XML、CLI3、
Replay API 资源/事件/像素与 22 个畸形捕获拒绝已通过；旧 v1–v4 的
T01/T78/T95/T97/T98/T99/T100 定向回放已通过。

复测命令：

```sh
bash util/buildscripts/scripts/test_metal_capture_batch101_macos.sh
RENDERDOC_METAL_LAST_TEST=101 bash util/buildscripts/scripts/test_metal_replay_batch35_38_macos.sh
```

完整回归日志 `/tmp/metal-batch101-full.log`：**101 captures、2434 畸形、
1010 lifecycle** 全通过，resident growth 3,538,944 bytes。T101 capture
SHA-256 `394b6a8f960b…`，replay 库/app 内嵌库 `77b615b94015…`。原始
剩余 **50 bridge / 40 旧 chunk**；T70 的 GPU 生成 ICB range 另计。
没有 GUI/Computer Use、提交或推送。

集中 GUI QA：打开 `t101_capture.rdc`，确认普通 VS/FS draw、render pass
sampleBuffer 资源引用、四个索引 0/1/2/3、随后的 `resolveCounters` Copy
事件以及目标 buffer 的四个非零递增时间戳。时间戳每次 replay 可以变化，
不要比较具体数值；在 resolve 事件、draw、再返回 resolve 之间 seek，
确认中心像素 RGB≈51/179/77 且 buffer 可读。无需另开一轮 UI 自动化。
