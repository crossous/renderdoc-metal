# BATCH84 — Object buffer/bytes 绑定重载

接通`setObjectBytes`、`setObjectBufferOffset`、`setObjectBuffers`三个bridge及
新chunk1303–1305。回放检查encoder、槽位/数组形状、inline长度、已有buffer
状态和offset在资源内；buffer按读前写引用标记。T83 `setObjectBuffer`也参与
同一夹具。object shader读取offset0/16/32及inline delta，生成payload并让
mesh shader在三次MeshDispatch把小三角形放在x≈80/220/360。Replay API逐
事件核对位置、背景和前后回退，13个畸形chunk拒绝。

- 定向：`bash util/buildscripts/scripts/test_metal_capture_batch84_macos.sh`；五次
  原生Metal Validation、捕获/XML、CLI3、API像素、负例及T78–T83回归已通过。
- 全量：`RENDERDOC_METAL_LAST_TEST=84 bash util/buildscripts/scripts/test_metal_replay_batch35_38_macos.sh`；
  84 captures、2141 malformed、840 lifecycle已通过，resident growth 1671168 bytes，
  日志`/tmp/metal-batch84-full.log`。
- 捕获SHA`d29d5e86c1a9…`、库/app SHA`aef27dba4f8d…`。
- 原始剩余**60 bridge / 42旧chunk**，T70实际缺口另计61/43。
  GUI/Computer Use未运行，51份capture待集中人工。
