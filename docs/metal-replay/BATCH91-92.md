# BATCH91–92 — 多线程 Object stage 的直接 Mesh 绘制

修复两个已接通 draw chunk 的过窄回放校验：object shader 存在时，不再把
`threadsPerObjectThreadgroup` 硬编码为 `1×1×1`，而是按实际 pipeline 的
`maxTotalThreadsPerObjectThreadgroup` 校验非零三维乘积且防溢出；
无 object stage 的 mesh pipeline 仍只接受被忽略参数 `1×1×1`。
Mesh stage 上限、grid 上限和资源身份校验保持不变。

- T91：Object→payload→Mesh 使用 `drawMeshThreadgroups`，
  `threadsPerObjectThreadgroup=4×1×1`。
- T92：同一 shader 使用 `drawMeshThreads`，
  `threadsPerGrid=4×1×1` 与 object threadgroup `4×1×1`。
- 联合定向：
  `bash util/buildscripts/scripts/test_metal_capture_batch91_92_macos.sh`。
  各五次原生 Metal Validation、两份capture/XML、CLI3、Replay API
  管线/中心像素及16个畸形输入拒绝，并回归 T78/T81/T83/T88/T90，已通过。
- 全量：
  `RENDERDOC_METAL_LAST_TEST=92 bash util/buildscripts/scripts/test_metal_replay_batch35_38_macos.sh`。
  92 captures、2231 malformed、920 lifecycle通过，resident growth 1884160 bytes；
  日志`/tmp/metal-batch92-full.log`。
- 捕获SHA：T91 `3ea72dbf14f6…`、T92 `e18b6eb53bb5…`；
  库及app内嵌库 `dd543ab096a7…`。
- 原始剩余 **52 bridge / 42旧chunk**，T70功能缺口另计53/43。
  本批是已有chunk语义修复，未冒充标记清除。GUI/Computer Use未运行；
  T34–T69、T71–T92 与 T10 marker 共59份待集中人工；未提交/推送。
