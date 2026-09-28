# BATCH80 — Mesh texture/sampler 绑定

接通mesh stage两种texture、四种sampler（含单/批LOD clamp）bridge及新chunk
1293–1298。回放验证encoder、槽位/数组形状、资源类型与设备、有限非负有序LOD；
资源按读引用标记。没有将mesh资源伪装为vertex stage状态。

T80复用T79三阶段位移mesh夹具，改用真实纹理采样shader。1×1 RGBA纹理红通道
178/255使三个三角形相对于T79各右移约70像素；Replay API逐事件检查新旧位置、
清屏与pipeline身份。每种重载至少捕获一次，28个畸形chunk拒绝。

- 定向：`bash util/buildscripts/scripts/test_metal_capture_batch80_macos.sh`；五次
  原生Metal Validation、捕获/XML、CLI3、API像素、负例、T78/T79回归已通过。
- 全量：`RENDERDOC_METAL_LAST_TEST=80 bash util/buildscripts/scripts/test_metal_replay_batch35_38_macos.sh`；
  80 captures、2096 malformed、800 lifecycle已通过，resident growth 1638400 bytes，
  日志`/tmp/metal-batch80-full.log`。
- 捕获SHA`28d188275e44…`、库/app SHA`54f1bbde30f5…`。
- 原始剩余**66 bridge / 42旧chunk**；T70功能缺口另计67/43。
  GUI/Computer Use未运行，47份capture待集中人工。
