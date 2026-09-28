# BATCH93–94 — Object 输入网格与 Mesh 输出网格分离

Metal SDK 的 `MTLMeshRenderPipelineDescriptor.maxTotalThreadgroupsPerMeshGrid`
定义的是每个 object shader 调用通过
`mesh_grid_properties::set_threadgroups_per_grid` 输出的 mesh 网格上限，
不是输入 object threadgroup 数。两个直接 draw 的回放校验此前将它错误套到
object 输入网格；现只在 mesh-only pipeline 上用该限制校验直接 mesh 网格。
带 object stage 时仍要求 pipeline 身份、输入维度非零且可表示、object/mesh
threadgroup 乘积在各自 pipeline 上限内；GPU 执行由原生 Metal Validation 检查。

- T93：`drawMeshThreadgroups(2×1×1)`，object threadgroup 4×1×1，
  shader 每组输出 1 个 mesh threadgroup；真实三角形。
- T94：`drawMeshThreads(64×1×1)`，object threadgroup 4×1×1，共16组；
  同样输出三角形。两个 fixture 均不把输入 grid 和输出 mesh grid 混同。
- 联合定向：
  `bash util/buildscripts/scripts/test_metal_capture_batch93_94_macos.sh`；
  各五次原生 Metal Validation、捕获/XML、CLI3、Replay API 像素/事件及
  16个畸形输入拒绝，回归 T81/T83/T90/T91/T92 已通过。
- 全量：
  `RENDERDOC_METAL_LAST_TEST=94 bash util/buildscripts/scripts/test_metal_replay_batch35_38_macos.sh`。
  94 captures、2247 malformed、940 lifecycle通过，resident growth 1327104 bytes；
  日志`/tmp/metal-batch94-full.log`。
- 捕获SHA：T93 `0b4476799c01…`、T94 `28f1ccb2592a…`；
  库及app内嵌库 `3612c349a613…`。
- 原始剩余 **52 bridge / 42旧chunk**；T70功能缺口另计53/43。
  本批修复已接通chunk的合法分支，不计为标记清除。GUI/Computer Use
  未运行；T34–T69、T71–T94 与 T10 marker 共61份待集中人工；未提交/推送。
