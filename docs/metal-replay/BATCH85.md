# BATCH85 — Object texture/sampler 六重载

接通对象阶段两种texture、四种sampler（含单/批LOD clamp）bridge与新chunk
1306–1311。回放严格检查encoder、槽位/数组形状、资源类型与设备、有限非负有序
LOD；texture/sampler按读引用标记。T85 object shader实际采样1×1 RGBA纹理
的红通道128/255，把偏移经payload传给mesh shader；三个事件三角形分别落在
x≈120/250/370。Replay API核对采样后位置、原位置黑色、背景和事件回退，
28个畸形chunk安全拒绝。

- 定向：`bash util/buildscripts/scripts/test_metal_capture_batch85_macos.sh`；五次
  原生Metal Validation、捕获/XML、CLI3、API像素、负例及T78–T84回归已通过。
- 全量：`RENDERDOC_METAL_LAST_TEST=85 bash util/buildscripts/scripts/test_metal_replay_batch35_38_macos.sh`；
  85 captures、2169 malformed、850 lifecycle已通过，resident growth 3096576 bytes，
  日志`/tmp/metal-batch85-full.log`。
- 捕获SHA`e3bf1eee74e6…`、库/app SHA`6091de8a8265…`。
- 原始剩余**54 bridge / 42旧chunk**；T70实际缺口另计55/43。
  GUI/Computer Use未运行，52份capture待集中人工。
