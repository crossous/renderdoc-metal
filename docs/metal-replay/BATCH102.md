# BATCH102 — Reflection MTLBufferBinding argument encoder

接通 `MTLDevice::newArgumentEncoderWithBufferBinding` bridge，新 chunk1316。
从 render pipeline reflection 的只读 `MTLBufferBinding` 快照中提取简单
texture2d/sampler 成员，重建同等 argument encoder；保存并在回放核对
`encodedLength`/`alignment`，拒绝未知/复杂成员布局而不猜测内存编码。
此批复用T60双packet夹具，在左右半屏由argument buffer成员分别采样
两张纹理，API验证成员身份、fragment资源/采样器、像素与事件回退。

复测命令：

```sh
bash util/buildscripts/scripts/test_metal_capture_batch102_macos.sh
RENDERDOC_METAL_LAST_TEST=102 bash util/buildscripts/scripts/test_metal_replay_batch35_38_macos.sh
```

定向已通过原生三轮、capture/XML、CLI3、Replay API、13个畸形捕获拒绝，
并复测T01/T57/T60/T95/T100/T101旧capture。完整回归日志
`/tmp/metal-batch102-full.log`：**102 captures、2447畸形、1020 lifecycle**
全通过，resident growth 2,375,680 bytes。T102 capture SHA-256
`b9df0c466069…`，库/app内嵌库`30a8ca932d9f…`。原始剩余 **49 bridge / 40旧chunk**，
T70的GPU生成ICB range另计。没有GUI/Computer Use、提交或推送。

集中GUI QA：打开`t102_capture.rdc`，确认创建chunk的两个成员（texture
id0、sampler id1）与长度16、对齐8、supported=true。两个draw分别
使用同一argument buffer的不同packet；x100 RGB≈20/40/60，x300
RGB≈70/80/90。前后seek时成员资源链接与像素应跟随。
