# BATCH86 — Object threadgroup memory

接通`setObjectThreadgroupMemoryLength` bridge与新chunk1312。回放校验encoder、
slot、16-byte对齐和设备动态threadgroup memory上限；原生object shader把buffer
与inline参数写入`threadgroup` scratch，再通过payload传给mesh shader。
三个render pass分别绑定16/32/48 bytes，三次MeshDispatch仍按offset绘制
三角形。Replay API逐事件核对长度、位置和回退；5个畸形chunk安全拒绝。

- 定向：`bash util/buildscripts/scripts/test_metal_capture_batch86_macos.sh`；五次
  原生Metal Validation、捕获/XML、CLI3、API像素、负例及T83–T85回归已通过。
- 全量：`RENDERDOC_METAL_LAST_TEST=86 bash util/buildscripts/scripts/test_metal_replay_batch35_38_macos.sh`；
  86 captures、2174 malformed、860 lifecycle已通过，resident growth 1933312 bytes，
  日志`/tmp/metal-batch86-full.log`。
- 捕获SHA`6b70534fc597…`、库/app SHA`fbc12b54dfb3…`。
- 原始剩余**53 bridge / 42旧chunk**，T70实际缺口另计54/43。
  GUI/Computer Use未运行，53份capture待集中人工。
